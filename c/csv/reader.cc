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
#include "csv/reader.h"
#include <stdint.h>
#include <stdlib.h>
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------

GenericReader::GenericReader(const PyObj& pyrdr) {
  nthreads = normalize_nthreads(pyrdr.attr("nthreads").as_int32());
  verbose = pyrdr.attr("verbose").as_bool() == 1;
  src_arg = pyrdr.attr("src");
  file_arg = pyrdr.attr("file");
  text_arg = pyrdr.attr("text");
  fileno = pyrdr.attr("fileno").as_int32();
  mbuf = nullptr;

  verbose = true; // temporary
}

GenericReader::~GenericReader() {
  if (mbuf) mbuf->release();
}

int32_t GenericReader::normalize_nthreads(int32_t nthreads) {
  int32_t maxth = omp_get_max_threads();
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;
  return nthreads;
}



//------------------------------------------------------------------------------

void GenericReader::open_input() {
  if (fileno > 0) {
    const char* src = src_arg.as_cstring();
    if (verbose) printf("  Using file %s opened at fd=%d\n", src, fileno);
    mbuf = new OvermapMemBuf(src, 1, fileno);
    if (verbose) printf("  File size: %zu\n", (mbuf->size() - 1));
    return;
  }
  const char* text = text_arg.as_cstring();
  if (text) {
    mbuf = new ExternalMemBuf(text);
    return;
  }
  const char* filename = file_arg.as_cstring();
  if (filename) {
    if (verbose) printf("  Opening file %s\n", filename);
    mbuf = new OvermapMemBuf(filename, 1);
    if (verbose) printf("  File opened, size: %zu\n", (mbuf->size() - 1));
    return;
  }
  throw RuntimeError() << "No input given to the GenericReader";
}

const char* GenericReader::dataptr() const {
  return static_cast<const char*>(mbuf->get());
}


std::unique_ptr<DataTable> GenericReader::read()
{
  open_input();

  {
    ArffReader arffreader(*this);
    auto dt = arffreader.read();
    if (dt) return dt;
  }

  return nullptr;
}




//------------------------------------------------------------------------------

template <typename TThreadContext> class ChunkedDataReader
{
  // the data is read from here:
  const char* inputptr;
  size_t inputsize;
  size_t inputline;
  // and saved into here:
  std::vector<OutputColumn> outcols;
  // via the intermediate buffers TThreadContext, that are instantiated within
  // the read_all() method.

  // Extra parameters:
  std::vector<ColumnSpec> colspec;
  size_t chunksize;
  size_t nchunks;
  int nthreads;
  bool contiguous_chunks;
  int : 24;

public:
  ChunkedDataReader();
  virtual ~ChunkedDataReader();
  virtual void compute_chunking_strategy();
  virtual void adjust_chunk_start(const char**);
  virtual void read_all();
  virtual void push_buffers(TThreadContext&);
  virtual const char* read_chunk(const char* start, const char* end,
                                 TThreadContext&);

  void set_input(const char* ptr, size_t size, size_t line);
};




//------------------------------------------------------------------------------
// StrBuf2
//------------------------------------------------------------------------------

StrBuf2::StrBuf2(int64_t i) {
  colidx = i;
  writepos = 0;
  usedsize = 0;
  allocsize = 1024;
  strdata = static_cast<char*>(malloc(allocsize));
  if (!strdata) {
    throw RuntimeError()
          << "Unable to allocate 1024 bytes for a temporary buffer";
  }
}

StrBuf2::~StrBuf2() {
  free(strdata);
}

void StrBuf2::resize(size_t newsize) {
  strdata = static_cast<char*>(realloc(strdata, newsize));
  allocsize = newsize;
  if (!strdata) {
    throw RuntimeError() << "Unable to allocate " << newsize
                         << " bytes for a temporary buffer";
  }
}



//------------------------------------------------------------------------------

template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::read_all()
{
  chunksize = inputsize / nchunks;
  const char* prev_chunkend = inputptr;
  bool stopTeam = false;

  #pragma omp parallel num_threads(nthreads)
  {
    int ithread = omp_get_thread_num();
    TThreadContext ctx(ithread);
    ctx.prepare_strbufs(colspec);

    #pragma omp for ordered schedule(dynamic)
    for (int ichunk = 0; ichunk < nchunks; ++ichunk) {
      if (stopTeam) continue;
      push_buffers(ctx);
      const char* chunkstart = inputptr + ichunk * chunksize;
      const char* chunkend = chunkstart + chunksize;
      if (ichunk == nchunks - 1) chunkend = inputptr + inputsize;
      if (ichunk > 0) adjust_chunk_start(&chunkstart);

      const char* end = read_chunk(chunkstart, chunkend, ctx);
      // ctx.preorder();

      #pragma omp ordered
      {
        if (chunkstart != prev_chunkend && contiguous_chunks) {
          printf("Previous chunk did not finish at the same place: %p vs %p...\n", prev_chunkend, chunkstart);
          // Re-read the chunk
          ctx.discard();
          end = read_chunk(prev_chunkend, chunkend, ctx);
        }
        prev_chunkend = end;
        ctx.order();
      }
    }
    ctx.push_buffers();
  }
}


template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::push_buffers(TThreadContext& ctx)
{
  if (ctx.used_nrows == 0) return;
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
  ctx.used_nrows = 0;
}


// Default implementation merely moves the pointer to the beginning of the
// next line.
template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::adjust_chunk_start(const char** pch) {
  const char* ch = *pch;
  while (*ch) {
    if (*ch == '\r' || *ch == '\n') {
      ch += 1 + (ch[0] + ch[1] == '\r' + '\n');
      break;
    }
    ch++;
  }
  *pch = ch;
}


template <typename TThreadContext>
void ChunkedDataReader<TThreadContext>::set_input(
    const char* input, size_t size, size_t line
) {
  inputptr = input;
  inputsize = size;
  inputline = line;
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


void ThreadContext::prepare_strbufs(const std::vector<ColumnSpec>& columns) {
  int64_t ncols = static_cast<int64_t>(columns.size());
  for (int64_t i = 0; i < ncols; ++i) {
    if (columns[i].type == ColumnSpec::Type::String) {
      strbufs.push_back(StrBuf2(i));
    }
  }
}


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
