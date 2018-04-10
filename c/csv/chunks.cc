//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include <algorithm>           // std::max
#include "csv/reader_fread.h"  // FreadReader, FreadLocalParseContext
#include "utils/assert.h"



//------------------------------------------------------------------------------
// ChunkedDataReader
//------------------------------------------------------------------------------

ChunkedDataReader::ChunkedDataReader(GenericReader& reader, double meanLineLen)
  : g(reader)
{
  chunkSize = 0;
  chunkCount = 0;
  inputStart = g.sof;
  inputEnd = g.eof;
  lastChunkEnd = inputStart;
  lineLength = std::max(meanLineLen, 1.0);
  nthreads = g.nthreads;
  allocnrow = g.columns.nrows();
  max_nrows = g.max_nrows;
  row0 = 0;
  xassert(allocnrow <= max_nrows);

  determine_chunking_strategy();
}


void ChunkedDataReader::determine_chunking_strategy() {
  size_t inputSize = static_cast<size_t>(inputEnd - inputStart);
  size_t size1000 = static_cast<size_t>(1000 * lineLength);
  size_t zThreads = static_cast<size_t>(nthreads);
  chunkSize = std::max<size_t>(size1000, 1 << 18);
  chunkCount = std::max<size_t>(inputSize / chunkSize, 1);
  if (chunkCount > zThreads) {
    chunkCount = zThreads * (1 + (chunkCount - 1)/zThreads);
  } else {
    nthreads = static_cast<int>(chunkCount);
    g.trace("Number of threads reduced to %d because data is small",
            nthreads);
  }
  chunkSize = inputSize / chunkCount;
}



ChunkCoordinates ChunkedDataReader::compute_chunk_boundaries(
  size_t i, LocalParseContext* ctx) const
{
  xassert(i < chunkCount);
  ChunkCoordinates c;

  bool isFirstChunk = (i == 0);
  bool isLastChunk = (i == chunkCount - 1);

  if (nthreads == 1 || isFirstChunk) {
    c.start = lastChunkEnd;
    c.true_start = true;
  } else {
    c.start = inputStart + i * chunkSize;
  }
  if (isLastChunk) {
    c.end = inputEnd;
    c.true_end = true;
  } else {
    c.end = c.start + chunkSize;
  }

  adjust_chunk_coordinates(c, ctx);

  return c;
}


bool ChunkedDataReader::is_ordered(
  const ChunkCoordinates& acc, ChunkCoordinates& xcc)
{
  bool ordered = (acc.start == lastChunkEnd);
  xcc.start = lastChunkEnd;
  xcc.true_start = true;
  if (ordered && acc.end) {
    xassert(acc.end >= lastChunkEnd);
    lastChunkEnd = acc.end;
  }
  return ordered;
}


double ChunkedDataReader::work_done_amount() const {
  double done = static_cast<double>(lastChunkEnd - inputStart);
  double total = static_cast<double>(inputEnd - inputStart);
  return done / total;
}


void ChunkedDataReader::adjust_chunk_coordinates(
      ChunkCoordinates&, LocalParseContext*) const {}




void ChunkedDataReader::read_all()
{
  // Any exceptions that are thrown within OMP blocks must be captured within
  // the same block. If allowed to propagate, they may corrupt the stack and
  // crash the program. This is why we have all code surrounded with
  // try-catch clauses, and `OmpExceptionManager` to help us remember the
  // exception that was thrown and manually propagate it outwards.
  OmpExceptionManager oem;
  bool progressShown = false;

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

    auto ctx = init_thread_context();
    ChunkCoordinates xcc;
    ChunkCoordinates acc;

    #pragma omp for ordered schedule(dynamic)
    for (size_t i = 0; i < chunkCount; i++) {
      if (oem.exception_caught()) continue;
      try {
        if (tShowAlways || (tShowProgress && wallclock() >= tShowWhen)) {
          g.progress(work_done_amount());
          tShowAlways = true;
        }

        ctx->push_buffers();
        xcc = compute_chunk_boundaries(i, ctx.get());
        ctx->read_chunk(xcc, acc);

      } catch (...) {
        oem.capture_exception();
      }

      #pragma omp ordered
      do {
        if (oem.exception_caught()) break;
        try {
          // The `is_ordered()` call checks whether the actual start of the
          // chunk was correct (i.e. there are no gaps/overlaps in the
          // input). If not, then we re-read the chunk using the correct
          // coordinates (which `is_ordered()` saves in variable `xcc`).
          // We also re-read if the first `read_chunk()` call returned an
          // error: even if the chunk's start was determined correctly, we
          // didn't know that up to this point, and so were not producing
          // the correct error message.
          // After re-reading, it is no longer possible to have `acc.end`
          // being nullptr: if a genuine reading error occurs, it will be
          // thrown as an exception. Thus, `acc.end` will have the correct
          // end of the current chunk, which MUST be reported to `chunkster`
          // by calling `is_ordered()` the second time.
          bool reparse_error = !acc.end && !xcc.true_start;
          if (!is_ordered(acc, xcc) || reparse_error) {
            xassert(xcc.true_start);
            ctx->read_chunk(xcc, acc);
            bool ok = acc.end && is_ordered(acc, xcc);
            xassert(ok);
          }
          ctx->row0 = row0;
          size_t new_row0 = row0 + ctx->used_nrows;
          if (new_row0 > allocnrow) {
            if (allocnrow == max_nrows) {
              // allocnrow is the same as max_nrows, no need to reallocate
              // the output, just truncate the rows in the current chunk.
              ctx->used_nrows = allocnrow - row0;
              new_row0 = allocnrow;
            } else {
              realloc_output_columns(i, new_row0);
            }
          }
          row0 = new_row0;

          ctx->orderBuffer();

        } catch (...) {
          oem.capture_exception();
        }
      } while (0);  // #pragma omp ordered
    }  // #pragma omp for ordered
    try {
      // Push out all buffers one last time.
      if (ctx->used_nrows) {
        if (oem.exception_caught()) {
          // Stopped early because of error. Discard the content of the
          // buffers, because they were not ordered, and trying to push them
          // may lead to unexpected bugs...
          ctx->used_nrows = 0;
        } else {
          ctx->push_buffers();
        }
      }
    } catch (...) {
      oem.capture_exception();
    }

    #pragma omp atomic update
    progressShown |= tShowAlways;

  }  // #pragma omp parallel

  if (progressShown) {
    int status = 1;
    if (oem.exception_caught()) {
      status++;
      try {
        oem.rethrow_exception_if_any();
      } catch (PyError& e) {
        status += e.is_keyboard_interrupt();
        oem.capture_exception();
      } catch (...) {
        oem.capture_exception();
      }
    }
    g.progress(work_done_amount(), status);
  }
  oem.rethrow_exception_if_any();
  xassert(row0 <= allocnrow || max_nrows <= allocnrow);

  // Reallocate the output to have the correct number of rows
  g.columns.allocate(row0);
}


void ChunkedDataReader::realloc_output_columns(size_t ichunk, size_t new_alloc)
{
  if (ichunk == chunkCount - 1) {
    // If we're on the last jump, then `new_alloc` is exactly how many rows
    // will be needed.
  } else {
    // Otherwise we adjust the alloc to account for future chunks as well.
    double exp_nrows = 1.2 * new_alloc * chunkCount / (ichunk + 1);
    new_alloc = std::max(static_cast<size_t>(exp_nrows), 1024 + allocnrow);
  }
  if (new_alloc > max_nrows) {
    new_alloc = max_nrows;
  }
  allocnrow = new_alloc;
  g.trace("Too few rows allocated, reallocating to %zu rows", allocnrow);

  dt::shared_lock(shmutex, /* exclusive = */ true);
  g.columns.allocate(allocnrow);
}
