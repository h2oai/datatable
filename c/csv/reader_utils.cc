//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include "csv/fread.h"   // temporary
#include "csv/reader_parsers.h"
#include "utils/assert.h"
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
  bool col_is_string = isstring();
  size_t allocsize = (nrows + col_is_string) * elemsize();
  if (mbuf) {
    mbuf->resize(allocsize);
  } else {
    mbuf = new MemoryMemBuf(allocsize);
  }
  if (col_is_string) {
    if (elemsize() == 4)
      mbuf->set_elem<int32_t>(0, -1);
    else
      mbuf->set_elem<int64_t>(0, -1);
    if (!strdata) {
      strdata = new MemoryWritableBuffer(allocsize);
    }
  }
}

const char* GReaderColumn::typeName() const {
  return ParserLibrary::info(type).name.data();
}

size_t GReaderColumn::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(type).elemsize);
}

bool GReaderColumn::isstring() const {
  return ParserLibrary::info(type).isstring();
}

MemoryBuffer* GReaderColumn::extract_databuf() {
  MemoryBuffer* r = mbuf;
  mbuf = nullptr;
  return r;
}

MemoryBuffer* GReaderColumn::extract_strbuf() {
  if (!(strdata && isstring())) return nullptr;
  // TODO: make get_mbuf() method available on WritableBuffer itself
  strdata->finalize();
  return strdata->get_mbuf();
}

size_t GReaderColumn::getAllocSize() const {
  return (mbuf? mbuf->size() : 0) +
         (strdata? strdata->size() : 0) +
         name.size() + sizeof(*this);
}


void GReaderColumn::convert_to_str64() {
  xassert(type == static_cast<int8_t>(PT::Str32));
  size_t nelems = mbuf->size() / sizeof(int32_t);
  MemoryBuffer* new_mbuf = new MemoryMemBuf(nelems * sizeof(int64_t));
  int32_t* old_data = static_cast<int32_t*>(mbuf->get());
  int64_t* new_data = static_cast<int64_t*>(new_mbuf->get());
  for (size_t i = 0; i < nelems; ++i) {
    new_data[i] = old_data[i];
  }
  type = static_cast<int8_t>(PT::Str64);
  mbuf->release();
  mbuf = new_mbuf->shallowcopy();
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
  const ParserInfo* parsers = ParserLibrary::get_parser_infos();
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = parsers[(*this)[i].type].code;
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = parsers[(*this)[i].type].code;
  }
  *ch = '\0';
  return out;
}

size_t GReaderColumns::nColumnsInOutput() const {
  size_t n = 0;
  for (const GReaderColumn& col : *this) {
    n += col.presentInOutput;
  }
  return n;
}

size_t GReaderColumns::nColumnsInBuffer() const {
  size_t n = 0;
  for (const GReaderColumn& col : *this) {
    n += col.presentInBuffer;
  }
  return n;
}

size_t GReaderColumns::nColumnsToReread() const {
  size_t n = 0;
  for (const GReaderColumn& col : *this) {
    n += col.typeBumped;
  }
  return n;
}

size_t GReaderColumns::nStringColumns() const {
  size_t n = 0;
  for (const GReaderColumn& col : *this) {
    n += col.isstring();
  }
  return n;
}

size_t GReaderColumns::totalAllocSize() const {
  size_t allocsize = sizeof(*this);
  for (const GReaderColumn& col : *this) {
    allocsize += col.getAllocSize();
  }
  return allocsize;
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
  xassert(used_nrows == 0);
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

