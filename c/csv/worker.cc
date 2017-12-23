//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "worker.h"
#include <limits>  // std::numeric_limits
#include "utils/exceptions.h"
#include "utils/omp.h"


//------------------------------------------------------------------------------
// GReaderOutputColumn
//------------------------------------------------------------------------------

GReaderOutputColumn::GReaderOutputColumn() {
  data = nullptr;
  type = 0;
}

GReaderOutputColumn::~GReaderOutputColumn() {
  if (data) data->release();
}

GReaderOutputStringColumn::GReaderOutputStringColumn() {
  strdata = nullptr;
}

GReaderOutputStringColumn::~GReaderOutputStringColumn() {
  delete strdata;
}



//------------------------------------------------------------------------------
// ChunkedDataReader
//------------------------------------------------------------------------------

ChunkedDataReader::ChunkedDataReader() {
  inputptr = nullptr;
  inputsize = 0;
  inputline = 1;
  chunksize = 0;
  nchunks = 0;
  chunks_contiguous = true;
  max_nrows = std::numeric_limits<size_t>::max();
  alloc_nrows = 0;
  nthreads = omp_get_max_threads();
}

ChunkedDataReader::~ChunkedDataReader() {}


void ChunkedDataReader::set_input(const char* ptr, size_t size, int64_t line) {
  inputptr = ptr;
  inputsize = size;
  inputline = line;
}


void ChunkedDataReader::compute_chunking_strategy() {
  if (nchunks == 0) {
    nchunks = nthreads <= 1? 1 : 3 * static_cast<size_t>(nthreads);
  }
  chunksize = inputsize / nchunks;
}


// Default implementation merely moves the pointer to the beginning of the
// next line.
const char* ChunkedDataReader::adjust_chunk_start(
    const char* ch, const char* end
) {
  while (ch < end) {
    if (*ch == '\r' || *ch == '\n') {
      ch += 1 + (ch[0] + ch[1] == '\r' + '\n');
      break;
    }
    ch++;
  }
  return ch;
}


void ChunkedDataReader::read_all()
{
  const char* const inputend = inputptr + inputsize;
  if (!inputptr || !inputend) return;
  assert(alloc_nrows <= max_nrows);
  //
  // Thread-common state
  // -------------------
  // last_chunkend
  //   The position where the last thread finished reading its chunk. This
  //   variable is only meaningful inside the "ordered" section, where the
  //   threads are ordered among themselves and the notion of "last thread" is
  //   well-defined. This variable is also used to ensure that all input content
  //   was properly read, and nothing was skipped.
  //
  //
  const char* last_chunkend = inputptr;
  bool stop_hard = false;
  bool stop_soft = false;
  size_t chunkdist = 0;
  size_t chunk0 = 0;
  size_t nrows_total = 0;

  #pragma omp parallel num_threads(nthreads)
  {
    #pragma omp master
    {
      nthreads = omp_get_num_threads();
      compute_chunking_strategy();  // sets nchunks and possibly chunksize
      assert(nchunks > 0);
      if (chunks_contiguous) {
        chunksize = chunkdist = inputsize / nchunks;
      } else {
        assert(chunksize > 0 && chunksize <= inputsize);
        if (nchunks > 1) {
          chunkdist = (inputsize - chunksize) / (nchunks - 1);
        }
      }
    }
    // Wait for chunking calculations, in case ThreadContext needs them...
    #pragma omp barrier

    ThreadContextPtr tctx = init_thread_context();
    const char* tend;
    size_t tnrows;

    while (last_chunkend < inputend)
    {
      #pragma omp for ordered schedule(dynamic) nowait
      for (size_t i = chunk0; i < nchunks; ++i) {
        if (stop_hard || stop_soft) continue;
        tctx->push_buffers();

        const char* chunkstart = inputptr + i * chunkdist;
        const char* chunkend = chunkstart + chunksize;
        if (i == nchunks - 1) chunkend = inputend;
        if (i > 0) chunkstart = adjust_chunk_start(chunkstart, chunkend);

        tend = tctx->read_chunk(chunkstart, chunkend);
        tnrows = tctx->get_nrows();
        assert(tend >= chunkend);

        // Artificial loop makes it easy to quickly exit the "ordered" section.
        #pragma omp ordered
        do {
          // If "hard stop" was requested by a previous thread while this thread
          // was waiting in the queue to enter the ordered section, then we
          // dismiss the current thread's data.
          if (stop_hard) {
            tctx->set_nrows(0);
            break;
          }
          // If `adjust_chunk_start()` above did not find the correct starting
          // point, then the data that was read is incorrect (and if reading
          // produced any errors, those might also be invalid). In this case we
          // simply disregard all the data read so far, and re-read the chunk
          // from the "correct" place. We are forced to do re-reading in a slow
          // way, blocking all other threads, but this case should really almost
          // never happen (and if it does, slight slowdown is a small price to
          // pay).
          if (chunkstart != last_chunkend && chunks_contiguous) {
            tctx->set_nrows(0);
            chunkstart = last_chunkend;
            tend = tctx->read_chunk(chunkstart, chunkend);
            tnrows = tctx->get_nrows();
          }
          size_t row0 = nrows_total;
          nrows_total += tnrows;
          last_chunkend = tend;
          // Since alloc_nrows never exceeds max_nrows, this if check is same as
          // ``nrows_total >= max_nrows || nrows_total > alloc_nrows``.
          if (nrows_total >= alloc_nrows) {
            // If this thread reaches or exceeds the requested max_nrows, then
            // no subsequent thread's data is needed: request a "hard stop".
            // However this thread's data still needs to be ordered and pushed.
            // Also it could be that `max_nrows > alloc_nrows`, and that case
            // has to be handled too (then we'll have both `stop_hard = true`
            // and `stop_soft = true`).
            if (nrows_total >= max_nrows) {
              tnrows -= nrows_total - max_nrows;
              nrows_total = max_nrows;
              tctx->set_nrows(tnrows);
              last_chunkend = inputend;
              stop_hard = true;
            }
            // If the total number of rows read exceeds the allocated storage
            // space, then request a "soft stop" so that the current and all
            // queued threads can safely finish reading their pieces. Note that
            // as subsequent threads enter the ordered section, all of them will
            // also reach this if clause
            if (nrows_total > alloc_nrows) {
              stop_soft = true;
              chunk0 = i + 1;
            }
          }
          // Allow each thread to perform any ordering it needs.
          tctx->order(row0);
        } while (0);
      }
      // Push buffers one last time
      tctx->push_buffers();
    }
  }
}


//------------------------------------------------------------------------------
// ThreadContext
//------------------------------------------------------------------------------

ThreadContext::ThreadContext(int ith, size_t nrows, size_t ncols) {
  ithread = ith;
  rowsize = 8 * ncols;
  wbuf_nrows = nrows;
  wbuf = malloc(rowsize * wbuf_nrows);
  used_nrows = 0;
  row0 = 0;
}


ThreadContext::~ThreadContext() {
  assert(used_nrows == 0);
  if (wbuf) free(wbuf);
}


// void ThreadContext::prepare_strbufs(const std::vector<ColumnSpec>& columns) {
//   size_t ncols = columns.size();
//   for (size_t i = 0; i < ncols; ++i) {
//     if (columns[i].type == ColumnSpec::Type::String) {
//       strbufs.push_back(StrBuf2(static_cast<int64_t>(i)));
//     }
//   }
// }


void* ThreadContext::next_row() {
  if (used_nrows == wbuf_nrows) {
    wbuf_nrows += (wbuf_nrows + 1) / 2;
    wbuf = realloc(wbuf, wbuf_nrows * rowsize);
    if (!wbuf) {
      throw RuntimeError() << "Unable to allocate " << wbuf_nrows * rowsize
                           << " bytes for the temporary buffers";
    }
  }
  return static_cast<void*>(static_cast<char*>(wbuf) + (used_nrows++) * rowsize);
}


void ThreadContext::push_buffers()
{
  if (used_nrows == 0) return;
  /*
  size_t rowsize8 = ctx.rowsize / 8;
  for (size_t i = 0, j = 0, k = 0; i < colspec.size(); ++i) {
    auto coltype = colspec[i].type;
    if (coltype == ColumnSpec::Type::Drop) {
      continue;
    }
    if (coltype == ColumnSpec::Type::String) {
      StrBuf2& sb = ctx.strbufs[k];
      outcols[j].strdata->write_at(sb.writepos, sb.usedsize, sb.strdata);
      sb.usedsize = 0;

      int32_t* dest = static_cast<int32_t*>(outcols[j].data);
      RelStr* src = static_cast<RelStr*>(ctx.wbuf);
      int32_t offset = abs(dest[-1]);
      for (int64_t row = 0; row < ctx.used_nrows; ++row) {
        int32_t o = src->offset;
        dest[row] = o >= 0? o + offset : o - offset;
        src += rowsize8;
      }
      k++;
    } else {
      // ... 3 cases, depending on colsize
    }
    j++;
  }
  */
  used_nrows = 0;
}
