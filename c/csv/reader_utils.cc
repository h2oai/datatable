//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include "csv/fread.h"   // temporary
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// GReaderColumn
//------------------------------------------------------------------------------

GReaderColumn::GReaderColumn() {
  mbuf = nullptr;
  strdata = nullptr;
  type = 0;
  typeBumped = false;
  presentInOutput = true;
  presentInBuffer = true;
}

GReaderColumn::GReaderColumn(GReaderColumn&& o)
  : mbuf(o.mbuf), name(std::move(o.name)), strdata(o.strdata), type(o.type),
    typeBumped(o.typeBumped), presentInOutput(o.presentInOutput),
    presentInBuffer(o.presentInBuffer) {
  o.mbuf = nullptr;
  o.strdata = nullptr;
}

GReaderColumn::~GReaderColumn() {
  if (mbuf) mbuf->release();
  delete strdata;
}


void GReaderColumn::allocate(size_t nrows) {
  if (!presentInOutput) return;
  bool isstring = (type == CT_STRING);
  size_t allocsize = (nrows + isstring) * elemsize();
  if (mbuf) {
    mbuf->resize(allocsize);
  } else {
    mbuf = new MemoryMemBuf(allocsize);
  }
  if (isstring) {
    mbuf->set_elem<int32_t>(0, -1);
    if (!strdata) {
      strdata = new MemoryWritableBuffer(allocsize);
    }
  }
}

size_t GReaderColumn::elemsize() const {
  return static_cast<size_t>(typeSize[type]);
}

MemoryBuffer* GReaderColumn::extract_databuf() {
  MemoryBuffer* r = mbuf;
  mbuf = nullptr;
  return r;
}

MemoryBuffer* GReaderColumn::extract_strbuf() {
  if (!(strdata && type == CT_STRING)) return nullptr;
  // TODO: make get_mbuf() method available on WritableBuffer itself
  strdata->finalize();
  return strdata->get_mbuf();
}

size_t GReaderColumn::getAllocSize() const {
  return (mbuf? mbuf->size() : 0) +
         (strdata? strdata->size() : 0) +
         name.size() + sizeof(*this);
}



//------------------------------------------------------------------------------
// GReaderColumns
//------------------------------------------------------------------------------

GReaderColumns::GReaderColumns() noexcept
    : std::vector<GReaderColumn>(), allocnrows(0) {}


void GReaderColumns::allocate(size_t nrows) {
  size_t ncols = size();
  for (size_t i = 0; i < ncols; ++i) {
    (*this)[i].allocate(nrows);
  }
  allocnrows = nrows;
}


std::unique_ptr<int8_t[]> GReaderColumns::getTypes() const {
  size_t n = size();
  std::unique_ptr<int8_t[]> res(new int8_t[n]);
  for (size_t i = 0; i < n; ++i) {
    res[i] = (*this)[i].type;
  }
  return res;
}

void GReaderColumns::setType(int8_t type) {
  size_t n = size();
  for (size_t i = 0; i < n; ++i) {
    (*this)[i].type = type;
  }
}

const char* GReaderColumns::printTypes() const {
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = typeSymbols[(*this)[i].type];
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = typeSymbols[(*this)[i].type];
  }
  *ch = '\0';
  return out;
}

size_t GReaderColumns::nOutputs() const {
  size_t nouts = 0;
  size_t ncols = size();
  for (size_t i = 0; i < ncols; ++i) {
    nouts += (*this)[i].presentInOutput;
  }
  return nouts;
}

size_t GReaderColumns::nStringColumns() const {
  size_t nstrs = 0;
  size_t ncols = size();
  for (size_t i = 0; i < ncols; ++i) {
    nstrs += ((*this)[i].type == CT_STRING);
  }
  return nstrs;
}

size_t GReaderColumns::totalAllocSize() const {
  size_t allocsize = sizeof(*this);
  size_t ncols = size();
  for (size_t i = 0; i < ncols; ++i) {
    allocsize += (*this)[i].getAllocSize();
  }
  return allocsize;
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
  bool stop_team = false;
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
    // Wait for chunking calculations, in case LocalParseContext needs them...
    #pragma omp barrier

    LocalParseContextPtr tctx = init_thread_context();
    const char* tend;
    size_t tnrows;

    start_reading:;
    #pragma omp for ordered schedule(dynamic) nowait
    for (size_t i = chunk0; i < nchunks; ++i) {
      if (stop_team) continue;
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
        if (stop_team && !stop_soft) {
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
            stop_team = true;
          }
          // If the total number of rows read exceeds the allocated storage
          // space, then request a "soft stop" so that the current and all
          // queued threads can safely finish reading their pieces. Note that
          // as subsequent threads enter the ordered section, all of them will
          // also reach this if clause, in the end `chunk0` will point to the
          // first unread chunk.
          if (nrows_total > alloc_nrows) {
            chunk0 = i + 1;
            stop_soft = true;
            stop_team = true;
          }
        }
        // Allow each thread to perform any ordering it needs.
        tctx->order(row0);
      } while (0);  // #omp ordered
    } // #omp for(i in chunk0..nchunks) nowait

    // Push the remaining data in the buffers
    if (!stop_team) {
      tctx->push_buffers();
    }

    // Make all threads wait at this point. We want to wait after the last
    // thread has pushed its buffers (hence "nowait" in the omp-for-loop above).
    // At the same time, the case "nrows_total > alloc_nrows" below requires
    // that no thread was accessing the global buffer which will be reallocared.
    #pragma omp barrier

    // When the number of rows read exceeds the amount of allocated rows, then
    // reallocate the output columns and go back to the top of the reading loop.
    // The reallocation is done from the master thread, while all other threads
    // wait until it is complete. However we do not want to leave the parallel
    // region so as not to lose the data in each thread's buffer.
    if (nrows_total > alloc_nrows) {
      #pragma omp master
      {
        size_t new_alloc_nrows = nrows_total;
        if (chunk0 < nchunks) {
          new_alloc_nrows = static_cast<size_t>(1.2 * nrows_total *
                                                nchunks / chunk0);
        }
        assert(new_alloc_nrows >= nrows_total);
        realloc_columns(new_alloc_nrows);
        alloc_nrows = new_alloc_nrows;
        stop_team = false;
        stop_soft = false;
      }
      #pragma omp barrier
      goto start_reading;
    }
  } // #omp parallel
}



//------------------------------------------------------------------------------
// LocalParseContext
//------------------------------------------------------------------------------

LocalParseContext::LocalParseContext(size_t ncols, size_t nrows) {
  tbuf = nullptr;
  tbuf_ncols = 0;
  tbuf_nrows = 0;
  used_nrows = 0;
  row0 = 0;
  allocate_tbuf(ncols, nrows);
}


void LocalParseContext::allocate_tbuf(size_t ncols, size_t nrows) {
  size_t old_size = tbuf? (tbuf_ncols * tbuf_nrows + 1) * sizeof(field64) : 0;
  size_t new_size = (ncols * nrows + 1) * sizeof(field64);
  if (new_size > old_size) {
    void* tbuf_raw = realloc(tbuf, new_size);
    if (!tbuf_raw) {
      throw MemoryError() << "Cannot allocate " << new_size
                          << " bytes for a temporary buffer";
    }
    tbuf = static_cast<field64*>(tbuf_raw);
  }
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
}


LocalParseContext::~LocalParseContext() {
  assert(used_nrows == 0);
  free(tbuf);
}


// void LocalParseContext::prepare_strbufs(const std::vector<ColumnSpec>& columns) {
//   size_t ncols = columns.size();
//   for (size_t i = 0; i < ncols; ++i) {
//     if (columns[i].type == ColumnSpec::Type::String) {
//       strbufs.push_back(StrBuf2(static_cast<int64_t>(i)));
//     }
//   }
// }


field64* LocalParseContext::next_row() {
  if (used_nrows == tbuf_nrows) {
    allocate_tbuf(tbuf_ncols, tbuf_nrows * 3 / 2);
  }
  return tbuf + (used_nrows++) * tbuf_ncols;
}


void LocalParseContext::push_buffers()
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
      RelStr* src = static_cast<RelStr*>(ctx.tbuf);
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

