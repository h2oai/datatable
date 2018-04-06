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
  inputStart = reader.sof;
  inputEnd = reader.eof;
  lastChunkEnd = inputStart;
  lineLength = std::max(meanLineLen, 1.0);
  nThreads = reader.nthreads;
  determine_chunking_strategy();

  allocnrow = g.columns.nrows();
  max_nrows = g.max_nrows;
  nthreads = g.nthreads;
  chunk0 = 0;
  row0 = 0;
  xassert(allocnrow <= max_nrows);
}


void ChunkedDataReader::determine_chunking_strategy() {
  size_t inputSize = static_cast<size_t>(inputEnd - inputStart);
  size_t size1000 = static_cast<size_t>(1000 * lineLength);
  size_t zThreads = static_cast<size_t>(nThreads);
  chunkSize = std::max<size_t>(size1000, 1 << 18);
  chunkCount = std::max<size_t>(inputSize / chunkSize, 1);
  if (chunkCount > zThreads) {
    chunkCount = zThreads * (1 + (chunkCount - 1)/zThreads);
  } else {
    nThreads = static_cast<int>(chunkCount);
  }
  chunkSize = inputSize / chunkCount;
}



size_t ChunkedDataReader::get_nchunks() const {
  return chunkCount;
}

int ChunkedDataReader::get_nthreads() const {
  return nThreads;
}

void ChunkedDataReader::set_nthreads(int nth) {
  xassert(nth > 0);
  nThreads = nth;
  determine_chunking_strategy();
}


ChunkCoordinates ChunkedDataReader::compute_chunk_boundaries(
  size_t i, LocalParseContext* ctx) const
{
  xassert(i < chunkCount);
  ChunkCoordinates c;

  bool isFirstChunk = (i == 0);
  bool isLastChunk = (i == chunkCount - 1);

  if (nThreads == 1 || isFirstChunk) {
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


void ChunkedDataReader::unorder_chunk(const ChunkCoordinates& cc) {
  assert(cc.end == lastChunkEnd);
  lastChunkEnd = cc.start;
}


double ChunkedDataReader::work_done_amount() const {
  double done = static_cast<double>(lastChunkEnd - inputStart);
  double total = static_cast<double>(inputEnd - inputStart);
  return done / total;
}


void ChunkedDataReader::adjust_chunk_coordinates(
      ChunkCoordinates&, LocalParseContext*) const {}




void ChunkedDataReader::read_all() {
  // If we need to restart reading the file because we ran out of allocation
  // space, then this variable will tell how many new rows has to be allocated.
  size_t extraAllocRows = 0;
  bool stopTeam = false;
  size_t nchunks = 0;
  bool progressShown = false;
  OmpExceptionManager oem;
  int new_nthreads = get_nthreads();
  if (new_nthreads != nthreads) {
    nthreads = new_nthreads;
    g.trace("Number of threads reduced to %d because data is small",
            nthreads);
  }

  #pragma omp parallel num_threads(nthreads)
  {
    bool tMaster = false;
    #pragma omp master
    {
      int actualNthreads = omp_get_num_threads();
      if (actualNthreads != nthreads) {
        nthreads = actualNthreads;
        set_nthreads(nthreads);
        g.trace("Actual number of threads allowed by OMP: %d", nthreads);
      }
      nchunks = get_nchunks();
      tMaster = true;
    }
    // Wait for master here: we want all threads to have consistent
    // view of the chunking parameters.
    #pragma omp barrier

    bool tShowProgress = g.report_progress && tMaster;
    bool tShowAlways = false;
    double tShowWhen = tShowProgress? wallclock() + 0.75 : 0;

    auto ctx = init_thread_context();
    ChunkCoordinates xcc;
    ChunkCoordinates acc;

    #pragma omp for ordered schedule(dynamic)
    for (size_t i = chunk0; i < nchunks; i++) {
      if (stopTeam) continue;
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
        stopTeam = true;
      }

      #pragma omp ordered
      do {
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
          ctx->row0 = row0;  // fetch shared row0 (where to write my results to the answer).
          if (ctx->row0 >= allocnrow) {  // a previous thread has already reached the `allocnrow` limit
            stopTeam = true;
            ctx->used_nrows = 0;
          } else if (ctx->used_nrows + ctx->row0 > allocnrow) {  // current thread has reached `allocnrow` limit
            if (allocnrow == max_nrows) {
              // allocnrow is the same as max_nrows, no need to reallocate the DT,
              // just truncate the rows in the current chunk.
              ctx->used_nrows = max_nrows - ctx->row0;
            } else {
              // We reached `allocnrow` limit, but there are more data to read
              // left. In this case we arrange to terminate all threads but
              // remember the position where the previous thread has finished. We
              // will reallocate the DT and restart reading from the same point.
              chunk0 = i;
              if (i < nchunks - 1) {
                extraAllocRows = (size_t)((double)(row0+ctx->used_nrows)*nchunks/(i+1) * 1.2) - allocnrow;
                if (extraAllocRows < 1024) extraAllocRows = 1024;
              } else {
                // If we're on the last jump, then we know exactly how many extra rows is needed.
                extraAllocRows = row0 + ctx->used_nrows - allocnrow;
              }
              ctx->used_nrows = 0; // do not push this chunk
              unorder_chunk(acc);
              stopTeam = true;
            }
          }
          row0 += ctx->used_nrows;
          if (!stopTeam) ctx->orderBuffer();

        } catch (...) {
          oem.capture_exception();
          stopTeam = true;
        }
      } while (0);  // #pragma omp ordered
    }  // #pragma omp for ordered
    try {
      // Push out all buffers one last time.
      if (ctx->used_nrows) {
        if (stopTeam && oem.exception_caught()) {
          // Stopped early because of error. Discard the content of the buffers,
          // because they were not ordered, and trying to push them may lead to
          // unexpected bugs...
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
  if (extraAllocRows) {
    allocnrow += extraAllocRows;
    if (allocnrow > max_nrows) allocnrow = max_nrows;
    g.trace("  Too few rows allocated. Allocating additional %llu rows "
            "(now nrows=%llu) and continue reading from jump point %zu",
            (llu)extraAllocRows, (llu)allocnrow, chunk0);
    g.columns.allocate(allocnrow);
    // Re-read starting from chunk0
    read_all();
  } else {
    g.columns.allocate(row0);
  }
}
