//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <algorithm>           // std::max
#include "csv/reader.h"
#include "parallel/api.h"
#include "read/parallel_reader.h"
#include "utils/assert.h"
#include "utils/misc.h"
namespace dt {
namespace read {



ParallelReader::ParallelReader(GenericReader& reader, double meanLineLen)
  : chunk_size(0),
    chunk_count(0),
    input_start(reader.sof),
    input_end(reader.eof),
    end_of_last_chunk(input_start),
    approximate_line_length(meanLineLen),
    g(reader),
    preframe(reader.preframe),
    nthreads(static_cast<size_t>(reader.nthreads))
{
  xassert(input_end >= input_start);
  determine_chunking_strategy();
}

ParallelReader::~ParallelReader() {}



void ParallelReader::determine_chunking_strategy() {
  size_t input_size = static_cast<size_t>(input_end - input_start);
  size_t nrows_max = g.max_nrows;

  double maxrows_size = static_cast<double>(nrows_max) * approximate_line_length;
  bool input_size_reduced = false;
  if (nrows_max < 1000000 && maxrows_size < static_cast<double>(input_size)) {
    input_size = static_cast<size_t>(maxrows_size * 1.5) + 1;
    input_size_reduced = true;
  }
  chunk_size = std::max<size_t>(
                std::min<size_t>(
                  std::max<size_t>(
                    static_cast<size_t>(1000 * approximate_line_length),
                    1 << 16),
                  1 << 20),
                static_cast<size_t>(10 * approximate_line_length)
              );
  chunk_count = std::max<size_t>(input_size / chunk_size, 1);
  if (chunk_count > nthreads) {
    chunk_count = nthreads * (1 + (chunk_count - 1)/nthreads);
    chunk_size = input_size / chunk_count;
  } else {
    nthreads = chunk_count;
    chunk_size = input_size / chunk_count;
    if (input_size_reduced) {
      // If chunk_count is 1 then we'll attempt to read the whole input
      // with the first chunk, which is not what we want...
      chunk_count += 2;
      if (g.verbose) {
        g.d() << "Number of threads reduced to " << nthreads
              << " because due to max_nrows=" << nrows_max
              << " we estimate the amount of data to be read will be small";
      }
    } else {
      if (g.verbose) {
        g.d() << "Number of threads reduced to " << nthreads
              << " because data is small";
      }
    }
  }
  if (g.verbose) {
    g.d() << "The input will be read in "
          << dt::log::plural(chunk_count, "chunk")
          << " of size " << chunk_size << " each";
  }
}



/**
 * Determine coordinates (start and end) of the `i`-th chunk. The index `i`
 * must be in the range [0..chunk_count).
 *
 * The optional `ThreadContext` instance may be needed for some
 * implementations of `ParallelReader` in order to perform additional
 * parsing using a thread-local context.
 *
 * The method `compute_chunk_boundaries()` may be called in-parallel,
 * assuming that different invocation receive different `ctx` objects.
 */
ChunkCoordinates ParallelReader::compute_chunk_boundaries(
    size_t i, ThreadContext* ctx) const
{
  xassert(i < chunk_count);
  ChunkCoordinates c;

  bool is_first_chunk = (i == 0);
  bool is_last_chunk = (i == chunk_count - 1);

  if (nthreads == 1 || is_first_chunk) {
    c.set_start_exact(end_of_last_chunk);
  } else {
    c.set_start_approximate(input_start + i * chunk_size);
  }

  // It is possible to reach the end of input before the last chunk (for
  // example if )
  const char* ch = c.get_start() + chunk_size;
  if (is_last_chunk || ch >= input_end) {
    c.set_end_exact(input_end);
  } else {
    c.set_end_approximate(ch);
  }

  adjust_chunk_coordinates(c, ctx);

  xassert(c.get_start() >= input_start && c.get_end() <= input_end);
  return c;
}



/**
 * Return the fraction of the input that was parsed, as a number between
 * 0 and 1.0.
 */
double ParallelReader::work_done_amount() const {
  double done = static_cast<double>(end_of_last_chunk - input_start);
  double total = static_cast<double>(input_end - input_start);
  return done / total;
}




/**
 * Main function that reads all data from the input.
 */
void ParallelReader::read_all()
{
  size_t pool_nthreads = dt::num_threads_in_pool();
  if (pool_nthreads < nthreads) {
    nthreads = pool_nthreads;
    if (g.verbose) {
      g.d() << "Actual number of threads: " << nthreads;
    }
    determine_chunking_strategy();
  }

  // TODO: merge with the ThreadContext class
  class OTask : public dt::OrderedTask {
    private:
      std::unique_ptr<ThreadContext> tctx;
      ParallelReader* rdr;
      // Helper variables for keeping track of chunk's coordinates:
      // `txcc` has the expected chunk coordinates (i.e. as determined ex ante
      // in `compute_chunk_coordinates()`), and `tacc` the actual chunk
      // coordinates (i.e. how much data was actually read in `read_chunk()`).
      // These two are very often the same; however when they do differ, it is
      // the job of `order_chunk()` to reconcile the differences.
      ChunkCoordinates txcc;
      ChunkCoordinates tacc;

    public:
      OTask(ParallelReader* reader)
        : tctx(reader->init_thread_context()),
          rdr(reader)
      { xassert(tctx); }

      void start(size_t i) override {
        txcc = rdr->compute_chunk_boundaries(i, tctx.get());

        // Read the chunk with the expected coordinates `txcc`. The actual
        // coordinates of the data read will be stored in variable `tacc`.
        // If the method fails with a recoverable error (such as a type
        // exception), it will return with `tacc.get_end() == nullptr`. If the
        // method fails without possibility of recovery, it will raise an
        // exception.
        tctx->read_chunk(txcc, tacc);
      }

      void order(size_t i) override {
        tctx->set_row0();

        // Re-read the chunk if its start was determined incorrectly
        auto prev_end = rdr->end_of_last_chunk;
        if (!(tacc.get_start() == prev_end && tacc.get_end() >= prev_end)) {
          txcc.set_start_exact(prev_end);
          tctx->read_chunk(txcc, tacc);  // updates `tacc`
          xassert(tacc.get_start() == prev_end && tacc.get_end() >= prev_end);
        }

        if (tctx->handle_typebumps(this)) return;

        rdr->end_of_last_chunk = tacc.get_end();
        size_t chunk_nrows = tctx->get_nrows();
        size_t new_nrows = tctx->ensure_output_nrows(chunk_nrows, i, this);
        if (new_nrows != chunk_nrows) {
          tctx->set_nrows(new_nrows);
          tctx->preorder();  // recalculate chunk stats, etc
        }
        tctx->order();
      }

      void finish(size_t) override {
        tctx->postorder();
      }
  };

  dt::parallel_for_ordered(
    /* n_iterations = */ chunk_count,
    NThreads(nthreads),
    [&] { return std::make_unique<OTask>(this); }
  );

  // Check that all input was read (unless interrupted early because of
  // nrows_max).
  if (preframe.nrows_written() < g.max_nrows) {
    xassert(end_of_last_chunk == input_end);
  }
  g.logger_.emit_pending_messages();
}





}}  // namespace dt::read
