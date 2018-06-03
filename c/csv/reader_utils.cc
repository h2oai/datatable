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
#include "python/string.h"
#include "python/long.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// GReaderColumn
//------------------------------------------------------------------------------

GReaderColumn::GReaderColumn() {
  strdata = nullptr;
  type = PT::Mu;
  rtype = RT::RAuto;
  typeBumped = false;
  presentInOutput = true;
  presentInBuffer = true;
}

GReaderColumn::GReaderColumn(GReaderColumn&& o)
  : mbuf(std::move(o.mbuf)), name(std::move(o.name)), strdata(o.strdata),
    type(o.type), rtype(o.rtype), typeBumped(o.typeBumped),
    presentInOutput(o.presentInOutput), presentInBuffer(o.presentInBuffer)
{
  o.strdata = nullptr;
}

GReaderColumn::~GReaderColumn() {
  delete strdata;
}


void GReaderColumn::allocate(size_t nrows) {
  if (!presentInOutput) return;
  bool col_is_string = isstring();
  size_t allocsize = (nrows + col_is_string) * elemsize();
  mbuf.resize(allocsize);
  if (col_is_string) {
    if (elemsize() == 4)
      mbuf.set_element<int32_t>(0, -1);
    else
      mbuf.set_element<int64_t>(0, -1);
    if (!strdata) {
      strdata = new MemoryWritableBuffer(allocsize);
    }
  }
}

const char* GReaderColumn::typeName() const {
  return ParserLibrary::info(type).name.data();
}

const std::string& GReaderColumn::get_name() const {
  return name;
}

const char* GReaderColumn::repr_name(const GenericReader& g) const {
  const char* start = name.c_str();
  const char* end = start + name.size();
  return g.repr_binary(start, end, 25);
}


size_t GReaderColumn::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(type).elemsize);
}

bool GReaderColumn::isstring() const {
  return ParserLibrary::info(type).isstring();
}

MemoryRange GReaderColumn::extract_databuf() {
  return std::move(mbuf);
}

MemoryRange GReaderColumn::extract_strbuf() {
  if (!(strdata && isstring())) return MemoryRange();
  // TODO: make get_mbuf() method available on WritableBuffer itself
  strdata->finalize();
  return strdata->get_mbuf();
}

size_t GReaderColumn::getAllocSize() const {
  return mbuf.memory_footprint() +
         (strdata? strdata->size() : 0) +
         name.size() + sizeof(*this);
}


void GReaderColumn::convert_to_str64() {
  xassert(type == PT::Str32);
  size_t nelems = mbuf.size() / sizeof(int32_t);
  MemoryRange new_mbuf(nelems * sizeof(int64_t));
  const int32_t* old_data = static_cast<const int32_t*>(mbuf.rptr());
  int64_t* new_data = static_cast<int64_t*>(new_mbuf.wptr());
  for (size_t i = 0; i < nelems; ++i) {
    new_data[i] = old_data[i];
  }
  type = PT::Str64;
  mbuf = std::move(new_mbuf);
}


PyTypeObject* GReaderColumn::NameTypePyTuple = nullptr;

void GReaderColumn::init_nametypepytuple() {
  if (NameTypePyTuple) return;
  static const char* tuple_name = "column_descriptor";
  static const char* field0 = "name";
  static const char* field1 = "type";
  PyStructSequence_Field* fields = new PyStructSequence_Field[3];
  fields[0].name = const_cast<char*>(field0);
  fields[1].name = const_cast<char*>(field1);
  fields[2].name = nullptr;
  fields[0].doc = nullptr;
  fields[1].doc = nullptr;
  fields[2].doc = nullptr;
  PyStructSequence_Desc* desc = new PyStructSequence_Desc;
  desc->name = const_cast<char*>(tuple_name);
  desc->doc = nullptr;
  desc->fields = fields;
  desc->n_in_sequence = 2;
  // Do not use PyStructSequence_NewType, because it is buggy
  // (see https://lists.gt.net/python/bugs/1320383)
  // The memory must also be cleared because https://bugs.python.org/issue33742
  NameTypePyTuple = new PyTypeObject();
  PyStructSequence_InitType(NameTypePyTuple, desc);

  // clean up
  delete[] fields;
  delete desc;
}


PyObj GReaderColumn::py_descriptor() const {
  if (!NameTypePyTuple) init_nametypepytuple();
  PyObject* nt_tuple = PyStructSequence_New(NameTypePyTuple);  // new ref
  if (!nt_tuple) throw PyError();
  PyObject* stype = py_stype_objs[ParserLibrary::info(type).stype];
  Py_INCREF(stype);
  PyStructSequence_SetItem(nt_tuple, 0, PyyString(name).release());
  PyStructSequence_SetItem(nt_tuple, 1, stype);
  return PyObj(std::move(nt_tuple));
}



//------------------------------------------------------------------------------
// GReaderColumns
//------------------------------------------------------------------------------

GReaderColumns::GReaderColumns() noexcept
    : std::vector<GReaderColumn>(), allocnrows(0) {}


size_t GReaderColumns::get_nrows() const {
  return allocnrows;
}

void GReaderColumns::set_nrows(size_t nrows) {
  size_t ncols = size();
  for (size_t i = 0; i < ncols; ++i) {
    (*this)[i].allocate(nrows);
  }
  allocnrows = nrows;
}


std::unique_ptr<PT[]> GReaderColumns::getTypes() const {
  std::unique_ptr<PT[]> res(new PT[size()]);
  saveTypes(res);
  return res;
}

void GReaderColumns::saveTypes(std::unique_ptr<PT[]>& types) const {
  size_t n = size();
  for (size_t i = 0; i < n; ++i) {
    types[i] = (*this)[i].type;
  }
}

bool GReaderColumns::sameTypes(std::unique_ptr<PT[]>& types) const {
  size_t n = size();
  for (size_t i = 0; i < n; ++i) {
    if (types[i] != (*this)[i].type) return false;
  }
  return true;
}

void GReaderColumns::setTypes(const std::unique_ptr<PT[]>& types) {
  size_t n = size();
  for (size_t i = 0; i < n; ++i) {
    (*this)[i].type = types[i];
  }
}

void GReaderColumns::setType(PT type) {
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
  if (used_nrows != 0) printf("used_nrows!=0 in ~LocalParseContext()\n");
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


