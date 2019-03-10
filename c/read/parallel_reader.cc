//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "read/parallel_reader.h"
#include <algorithm>           // std::max
#include "csv/reader.h"
#include "utils/assert.h"
#include "utils/parallel.h"

extern double wallclock();

namespace dt {
namespace read {



ParallelReader::ParallelReader(GenericReader& reader, double meanLineLen)
  : g(reader)
{
  chunkSize = 0;
  chunkCount = 0;
  inputStart = g.sof;
  inputEnd = g.eof;
  end_of_last_chunk = inputStart;
  lineLength = std::max(meanLineLen, 1.0);
  nthreads = g.nthreads;
  nrows_written = 0;
  nrows_allocated = g.columns.get_nrows();
  nrows_max = g.max_nrows;
  xassert(nrows_allocated <= nrows_max);

  determine_chunking_strategy();
}


void ParallelReader::determine_chunking_strategy() {
  size_t inputSize = static_cast<size_t>(inputEnd - inputStart);
  size_t zThreads = static_cast<size_t>(nthreads);
  double maxrowsSize = nrows_max * lineLength;
  bool inputSize_reduced = false;
  if (nrows_max < 1000000 && maxrowsSize < inputSize) {
    inputSize = static_cast<size_t>(maxrowsSize * 1.5) + 1;
    inputSize_reduced = true;
  }
  chunkSize = std::max<size_t>(
                std::min<size_t>(
                  std::max<size_t>(
                    static_cast<size_t>(1000 * lineLength),
                    1 << 16),
                  1 << 20),
                static_cast<size_t>(10 * lineLength)
              );
  chunkCount = std::max<size_t>(inputSize / chunkSize, 1);
  if (chunkCount > zThreads) {
    chunkCount = zThreads * (1 + (chunkCount - 1)/zThreads);
    chunkSize = inputSize / chunkCount;
  } else {
    nthreads = static_cast<int>(chunkCount);
    chunkSize = inputSize / chunkCount;
    if (inputSize_reduced) {
      // If chunkCount is 1 then we'll attempt to read the whole input
      // with the first chunk, which is not what we want...
      chunkCount += 2;
      g.trace("Number of threads reduced to %d because due to max_nrows=%zu "
              "we estimate the amount of data to be read will be small",
              nthreads, nrows_max);
    } else {
      g.trace("Number of threads reduced to %d because data is small",
              nthreads);
    }
  }
  g.trace("The input will be read in %zu chunks of size %zu each",
          chunkCount, chunkSize);
}



ChunkCoordinates ParallelReader::compute_chunk_boundaries(
  size_t i, ThreadContext* ctx) const
{
  xassert(i < chunkCount);
  ChunkCoordinates c;

  bool isFirstChunk = (i == 0);
  bool isLastChunk = (i == chunkCount - 1);

  if (nthreads == 1 || isFirstChunk) {
    c.set_start_exact(end_of_last_chunk);
  } else {
    c.set_start_approximate(inputStart + i * chunkSize);
  }

  // It is possible to reach the end of input before the last chunk (for
  // example if )
  const char* ch = c.get_start() + chunkSize;
  if (isLastChunk || ch >= inputEnd) {
    c.set_end_exact(inputEnd);
  } else {
    c.set_end_approximate(ch);
  }

  adjust_chunk_coordinates(c, ctx);

  xassert(c.get_start() >= inputStart && c.get_end() <= inputEnd);
  return c;
}



/**
 * Return the fraction of the input that was parsed, as a number between
 * 0 and 1.0.
 */
double ParallelReader::work_done_amount() const {
  double done = static_cast<double>(end_of_last_chunk - inputStart);
  double total = static_cast<double>(inputEnd - inputStart);
  return done / total;
}



void ParallelReader::adjust_chunk_coordinates(
      ChunkCoordinates&, ThreadContext*) const {}




void ParallelReader::read_all()
{
  // Any exceptions that are thrown within OMP blocks must be captured within
  // the same block. If allowed to propagate, they may corrupt the stack and
  // crash the program. This is why we have all code surrounded with
  // try-catch clauses, and `OmpExceptionManager` to help us remember the
  // exception that was thrown and manually propagate it outwards.
  OmpExceptionManager oem;

  #pragma omp parallel num_threads(nthreads)
  {
    bool tMaster = false;
    #pragma omp master
    {
      int actual_nthreads = omp_get_num_threads();
      if (actual_nthreads != nthreads) {
        nthreads = actual_nthreads;
        g.trace("Actual number of threads allowed by OMP: %d", nthreads);
        determine_chunking_strategy();
      }
      tMaster = true;
    }
    // Wait for master here: we want all threads to have consistent
    // view of the chunking parameters.
    #pragma omp barrier

    // These variables control how the progress bar is shown. `tShowProgress`
    // is the main flag telling us whether the progress bar should be shown
    // or not by the current thread (note that only master thread can have
    // this flag on -- this is because progress-reporting reaches to python
    // runtime, and we can do that from a single thread only). When
    // `tShowProgress` is on, the flag `tShowAlways` controls whether we need
    // to show the progress right away, or wait until time moment `tShowWhen`.
    // This is needed because we don't want the progress bar for really small
    // and fast files. However if the file is big enough (>256MB) then it's ok
    // to show the progress as soon as possible.
    bool tShowProgress = g.report_progress && tMaster;
    bool tShowAlways = tShowProgress && (inputEnd - inputStart > (1 << 28));
    double tShowWhen = tShowProgress? wallclock() + 0.75 : 0;

    // Thread-local parse context. This object does most of the parsing job.
    auto tctx = init_thread_context();

    // Helper variables for keeping track of chunk's coordinates:
    // `txcc` has the expected chunk coordinates (i.e. as determined ex ante
    // in `compute_chunk_coordinates()`), and `tacc` the actual chunk
    // coordinates (i.e. how much data was actually read in `read_chunk()`).
    // These two are very often the same; however when they do differ, it is
    // the job of `order_chunk()` to reconcile the differences.
    ChunkCoordinates txcc;
    ChunkCoordinates tacc;

    // Main data reading loop
    #pragma omp for ordered schedule(dynamic)
    for (size_t i = 0; i < chunkCount; ++i) {
      if (oem.stop_requested()) continue;
      try {
        if (tMaster) g.emit_delayed_messages();
        if (tShowAlways || (tShowProgress && wallclock() >= tShowWhen)) {
          g.progress(work_done_amount());
          tShowAlways = true;
        }

        tctx->push_buffers();
        txcc = compute_chunk_boundaries(i, tctx.get());
        tctx->read_chunk(txcc, tacc);

      } catch (...) {
        oem.capture_exception();
      }

      #pragma omp ordered
      do {
        if (oem.stop_requested()) {
          tctx->used_nrows = 0;
          break;
        }
        try {
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
              oem.stop_iterations();
            } else {
              realloc_output_columns(i, nrows_new);
            }
          }
          nrows_written = nrows_new;

          tctx->orderBuffer();

        } catch (...) {
          oem.capture_exception();
        }
      } while (0);  // #pragma omp ordered
    }  // #pragma omp for ordered

    // Stopped early because of error. Discard the content of the buffers,
    // because they were not ordered, and trying to push them may lead to
    // unexpected bugs...
    if (oem.exception_caught()) {
      tctx->used_nrows = 0;
    }

    // Push out the buffers one last time.
    if (tctx->used_nrows) {
      try {
        tctx->push_buffers();
      } catch (...) {
        tctx->used_nrows = 0;
        oem.capture_exception();
      }
    }

    // Report progress one last time
    if (tMaster) g.emit_delayed_messages();
    if (tShowAlways) {
      int status = 1 + oem.exception_caught() + oem.is_keyboard_interrupt();
      g.progress(work_done_amount(), status);
    }
  }  // #pragma omp parallel

  // If any exception occurred, propagate it to the caller
  oem.rethrow_exception_if_any();

  // Reallocate the output to have the correct number of rows
  g.columns.set_nrows(nrows_written);

  // Check that all input was read (unless interrupted early because of
  // nrows_max).
  if (nrows_written < nrows_max) {
    xassert(end_of_last_chunk == inputEnd);
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
  xassert(ichunk < chunkCount);
  if (new_nrows == nrows_allocated) {
    return;
  }
  if (ichunk == chunkCount - 1) {
    // If we're on the last jump, then `new_nrows` is exactly how many rows
    // will be needed.
  } else {
    // Otherwise we adjust the alloc to account for future chunks as well.
    double expected_nrows = 1.2 * new_nrows * chunkCount / (ichunk + 1);
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



}  // namespace read
}  // namespace dt
