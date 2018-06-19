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

LocalParseContext::LocalParseContext(size_t ncols, size_t nrows)
  : tbuf(ncols * nrows + 1), sbuf(0), strinfo(ncols)
{
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
  used_nrows = 0;
  row0 = 0;
}


LocalParseContext::~LocalParseContext() {
  if (used_nrows != 0) {
    printf("Assertion error in ~LocalParseContext(): used_nrows != 0\n");
  }
}


void LocalParseContext::allocate_tbuf(size_t ncols, size_t nrows) {
  tbuf.resize(ncols * nrows + 1);
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
}


size_t LocalParseContext::get_nrows() const {
  return used_nrows;
}


void LocalParseContext::set_nrows(size_t n) {
  xassert(n <= used_nrows);
  used_nrows = n;
}

