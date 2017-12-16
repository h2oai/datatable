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
void ChunkedDataReader::adjust_chunk_start(const char*& ch, const char* eof) {
  while (ch < eof) {
    if (*ch == '\r' || *ch == '\n') {
      ch += 1 + (ch[0] + ch[1] == '\r' + '\n');
      break;
    }
    ch++;
  }
}


void ChunkedDataReader::read_all()
{
  if (!inputptr || !inputsize) return;
  const char* prev_chunkend = inputptr;
  bool stopTeam = false;
  size_t chunkdist = 0;

  #pragma omp parallel num_threads(nthreads)
  {
    #pragma omp master
    {
      nthreads = omp_get_num_threads();
      compute_chunking_strategy();
      assert(nchunks > 0);
      chunkdist = chunks_contiguous? chunksize : inputsize / nchunks;
    }
    int ithread = omp_get_thread_num();
    ThreadContextPtr ctx = init_thread_context(ithread);

    #pragma omp for ordered schedule(dynamic)
    for (size_t ichunk = 0; ichunk < nchunks; ++ichunk) {
      if (stopTeam) continue;
      ctx->push_buffers();

      const char* chunkstart = inputptr + ichunk * chunkdist;
      const char* chunkend = chunkstart + chunksize;
      if (ichunk == nchunks - 1) chunkend = inputptr + inputsize;
      if (ichunk > 0) adjust_chunk_start(chunkstart, chunkend);

      const char* end = ctx->read_chunk(chunkstart, chunkend);

      #pragma omp ordered
      {
        if (chunkstart != prev_chunkend && chunks_contiguous) {
          // printf("Previous chunk did not finish at the same place: %p vs %p...\n", prev_chunkend, chunkstart);
          // Re-read the chunk
          ctx->discard();
          end = ctx->read_chunk(prev_chunkend, chunkend);
        }
        prev_chunkend = end;
        ctx->order();
      }
    }
    // Push buffers one last time
    ctx->push_buffers();
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


void ThreadContext::discard() {
  used_nrows = 0;
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
