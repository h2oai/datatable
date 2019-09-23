//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <algorithm>           // std::max
#include "csv/reader.h"
#include "parallel/api.h"
#include "read/parallel_reader.h"
#include "utils/assert.h"

extern double wallclock();

namespace dt {
namespace read {



ParallelReader::ParallelReader(GenericReader& reader, double meanLineLen)
  : g(reader)
{
  chunk_size = 0;
  chunk_count = 0;
  input_start = g.sof;
  input_end = g.eof;
  end_of_last_chunk = input_start;
  approximate_line_length = std::max(meanLineLen, 1.0);
  nthreads = static_cast<size_t>(g.nthreads);
  nrows_written = 0;
  nrows_allocated = g.columns.get_nrows();
  nrows_max = g.max_nrows;
  xassert(nrows_allocated <= nrows_max);

  determine_chunking_strategy();
}

ParallelReader::~ParallelReader() {}



void ParallelReader::determine_chunking_strategy() {
  size_t input_size = static_cast<size_t>(input_end - input_start);
  double maxrows_size = nrows_max * approximate_line_length;
  bool input_size_reduced = false;
  if (nrows_max < 1000000 && maxrows_size < input_size) {
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
      g.trace("Number of threads reduced to %zu because due to max_nrows=%zu "
              "we estimate the amount of data to be read will be small",
              nthreads, nrows_max);
    } else {
      g.trace("Number of threads reduced to %zu because data is small",
              nthreads);
    }
  }
  g.trace("The input will be read in %zu chunks of size %zu each",
          chunk_count, chunk_size);
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
    size_t i, ThreadContextPtr& ctx) const
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
    g.trace("Actual number of threads: %zu", nthreads);
    determine_chunking_strategy();
  }

  dt::parallel_for_ordered(
    /* n_iterations = */ chunk_count,
    /* n_threads = */ NThreads(nthreads),

    [&](dt::ordered* o) {
      // Thread-local parse context. This object does most of the parsing job.
      auto tctx = init_thread_context();
      xassert(tctx);

      // Helper variables for keeping track of chunk's coordinates:
      // `txcc` has the expected chunk coordinates (i.e. as determined ex ante
      // in `compute_chunk_coordinates()`), and `tacc` the actual chunk
      // coordinates (i.e. how much data was actually read in `read_chunk()`).
      // These two are very often the same; however when they do differ, it is
      // the job of `order_chunk()` to reconcile the differences.
      ChunkCoordinates txcc;
      ChunkCoordinates tacc;

      // Main data reading loop
      o->parallel(
        [&](size_t i) {
          if (dt::this_thread_index() == 0) {
            g.emit_delayed_messages();
          }

          txcc = compute_chunk_boundaries(i, tctx);

          // Read the chunk with the expected coordinates `txcc`. The actual
          // coordinates of the data read will be stored in variable `tacc`.
          // If the method fails with a recoverable error (such as a type
          // exception), it will return with `tacc.get_end() == nullptr`. If the
          // method fails without possibility of recovery, it will raise an
          // exception.
          tctx->read_chunk(txcc, tacc);
        },

        [&](size_t i) {
          tctx->row0 = nrows_written;
          order_chunk(tacc, txcc, tctx);

          size_t nrows_new = nrows_written + tctx->used_nrows;
          if (nrows_new > nrows_allocated) {
            if (nrows_new > nrows_max) {
              // more rows read than nrows_max, no need to reallocate
              // the output, just truncate the rows in the current chunk.
              xassert(nrows_max >= nrows_written);
              tctx->used_nrows = nrows_max - nrows_written;
              nrows_new = nrows_max;
              realloc_output_columns(i, nrows_new);
              o->set_n_iterations(i + 1);
            } else {
              realloc_output_columns(i, nrows_new);
            }
          }
          nrows_written = nrows_new;

          tctx->orderBuffer();
        },

        [&](size_t) {
          tctx->push_buffers();
        }
      );  // o->parallel()
    });  // dt::parallel_for_ordered
  g.emit_delayed_messages();

  // Reallocate the output to have the correct number of rows
  g.columns.set_nrows(nrows_written);

  // Check that all input was read (unless interrupted early because of
  // nrows_max).
  if (nrows_written < nrows_max) {
    xassert(end_of_last_chunk == input_end);
  }
}



/**
 * Reallocate output columns (i.e. `g.columns`) to the new number of rows.
 * Argument `ichunk` contains the index of the chunk that was read last (this
 * helps with determining the new number of rows), and `new_allocnrow` is the
 * minimal number of rows to reallocate to.
 *
 * This method is thread-safe: it will acquire an exclusive lock before
 * making any changes.
 */
void ParallelReader::realloc_output_columns(size_t ichunk, size_t new_nrows)
{
  xassert(ichunk < chunk_count);
  if (new_nrows == nrows_allocated) {
    return;
  }
  if (ichunk == chunk_count - 1) {
    // If we're on the last jump, then `new_nrows` is exactly how many rows
    // will be needed.
  } else {
    // Otherwise we adjust the alloc to account for future chunks as well.
    double expected_nrows = 1.2 * new_nrows * chunk_count / (ichunk + 1);
    new_nrows = std::max(static_cast<size_t>(expected_nrows),
                         1024 + nrows_allocated);
  }
  if (new_nrows > nrows_max) {
    // If the user asked to read no more than `nrows_max` rows, then there is
    // no point in allocating more than that amount.
    new_nrows = nrows_max;
  }
  nrows_allocated = new_nrows;

  g.trace("Too few rows allocated, reallocating to %zu rows", nrows_allocated);

  { // Acquire a lock and then resize all columns
    shared_lock<shared_mutex> lock(shmutex, /* exclusive = */ true);
    g.columns.set_nrows(nrows_allocated);
  }
}



/**
 * Ensure that the chunks were placed properly.
 *
 * This method must be called from the #ordered section. It takes three
 * arguments:
 *   - `acc` the *actual* coordinates of the chunk just read;
 *   - `xcc` the coordinates that were *expected*; and
 *   - `ctx` the thread-local parse context.
 *
 * If the chunk was ordered properly (i.e. started reading from the place
 * where the previous chunk ended), then this method updates the internal
 * `end_of_last_chunk` variable and returns.
 *
 * Otherwise, it re-parses the chunk with correct coordinates. When doing
 * so, it will set `xcc.start_exact` to true, thus informing the chunk
 * parser that the coordinates that it received are true.
 */
void ParallelReader::order_chunk(
    ChunkCoordinates& acc, ChunkCoordinates& xcc, ThreadContextPtr& ctx)
{
  for (int i = 0; i < 2; ++i) {
    if (acc.get_start() == end_of_last_chunk &&
        acc.get_end() >= end_of_last_chunk)
    {
      end_of_last_chunk = acc.get_end();
      return;
    }
    xassert(i == 0);
    xcc.set_start_exact(end_of_last_chunk);

    ctx->read_chunk(xcc, acc);  // updates `acc`
  }
}



}}  // namespace dt::read
