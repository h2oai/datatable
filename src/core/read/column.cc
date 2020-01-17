//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "read/column.h"
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "python/string.h"

namespace dt {
namespace read {



//---- Constructors ------------------------------------------------------------

Column::Column() {
  strbuf = nullptr;
  ptype = PT::Mu;
  rtype = RT::RAuto;
  typeBumped = false;
  presentInOutput = true;
  presentInBuffer = true;
}

Column::Column(Column&& o)
  : name(std::move(o.name)), databuf(std::move(o.databuf)), strbuf(o.strbuf),
    ptype(o.ptype), rtype(o.rtype), typeBumped(o.typeBumped),
    presentInOutput(o.presentInOutput), presentInBuffer(o.presentInBuffer)
{
  o.strbuf = nullptr;
}

Column::~Column() {
  delete strbuf;
}



//---- Column's data -----------------------------------------------------------

void Column::allocate(size_t nrows) {
  if (!presentInOutput) return;
  bool col_is_string = is_string();
  size_t allocsize = (nrows + col_is_string) * elemsize();
  databuf.resize(allocsize);
  if (col_is_string) {
    if (elemsize() == 4)
      databuf.set_element<int32_t>(0, 0);
    else
      databuf.set_element<int64_t>(0, 0);
    if (!strbuf) {
      strbuf = new MemoryWritableBuffer(allocsize);
    }
  }
}

void* Column::data_w() {
  return databuf.wptr();
}

WritableBuffer* Column::strdata_w() {
  return strbuf;
}

Buffer Column::extract_databuf() {
  return std::move(databuf);
}

Buffer Column::extract_strbuf() {
  if (!(strbuf && is_string())) return Buffer();
  strbuf->finalize();
  return strbuf->get_mbuf();
}



//---- Column's name -----------------------------------------------------------

const std::string& Column::get_name() const noexcept {
  return name;
}

void Column::set_name(std::string&& newname) noexcept {
  name = std::move(newname);
}

void Column::swap_names(Column& other) noexcept {
  name.swap(other.name);
}

const char* Column::repr_name(const GenericReader& g) const {
  const char* start = name.c_str();
  const char* end = start + name.size();
  return g.repr_binary(start, end, 25);
}



//---- Column's type -----------------------------------------------------------

PT Column::get_ptype() const {
  return ptype;
}

SType Column::get_stype() const {
  return ParserLibrary::info(ptype).stype;
}

Column::ptype_iterator
Column::get_ptype_iterator(int8_t* qr_ptr) const {
  return Column::ptype_iterator(ptype, rtype, qr_ptr);
}

void Column::set_ptype(const Column::ptype_iterator& it) {
  xassert(rtype == it.get_rtype());
  ptype = *it;
  typeBumped = true;
}

// Set .ptype to the provided value, disregarding the restrictions imposed
// by the .rtype field.
void Column::force_ptype(PT new_ptype) {
  ptype = new_ptype;
}

void Column::set_rtype(int64_t it) {
  rtype = static_cast<RT>(it);
  // Temporary
  switch (rtype) {
    case RDrop:
      ptype = PT::Str32;
      presentInOutput = false;
      presentInBuffer = false;
      break;
    case RAuto:    break;
    case RBool:    ptype = PT::Bool01; break;
    case RInt:     ptype = PT::Int32; break;
    case RInt32:   ptype = PT::Int32; break;
    case RInt64:   ptype = PT::Int64; break;
    case RFloat:   ptype = PT::Float32Hex; break;
    case RFloat32: ptype = PT::Float32Hex; break;
    case RFloat64: ptype = PT::Float64Plain; break;
    case RStr:     ptype = PT::Str32; break;
    case RStr32:   ptype = PT::Str32; break;
    case RStr64:   ptype = PT::Str64; break;
  }
}

const char* Column::typeName() const {
  return ParserLibrary::info(ptype).name.data();
}



//---- Column info -------------------------------------------------------------

bool Column::is_string() const {
  return ParserLibrary::info(ptype).isstring();
}

bool Column::is_dropped() const {
  return rtype == RT::RDrop;
}

bool Column::is_type_bumped() const {
  return typeBumped;
}

bool Column::is_in_output() const {
  return presentInOutput;
}

bool Column::is_in_buffer() const {
  return presentInBuffer;
}

size_t Column::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(ptype).elemsize);
}

void Column::reset_type_bumped() {
  typeBumped = false;
}

void Column::set_in_buffer(bool f) {
  presentInBuffer = f;
}



//---- Misc --------------------------------------------------------------------

static PyTypeObject* init_nametypepytuple() {
  static const char* tuple_name = "column_descriptor";
  static const char* field0 = "name";
  static const char* field1 = "type";
  PyStructSequence_Desc desc;
  PyStructSequence_Field fields[3];
  fields[0].name = const_cast<char*>(field0);
  fields[1].name = const_cast<char*>(field1);
  fields[2].name = nullptr;
  fields[0].doc = nullptr;
  fields[1].doc = nullptr;
  fields[2].doc = nullptr;
  desc.name = const_cast<char*>(tuple_name);
  desc.doc = nullptr;
  desc.fields = fields;
  desc.n_in_sequence = 2;
  // Do not use PyStructSequence_NewType, because it is buggy
  // (see https://lists.gt.net/python/bugs/1320383)
  // The memory must also be cleared because https://bugs.python.org/issue33742
  auto res = new PyTypeObject();
  PyStructSequence_InitType(res, &desc);
  return res;
}

py::oobj Column::py_descriptor() const {
  static PyTypeObject* name_type_pytuple = init_nametypepytuple();
  PyObject* nt_tuple = PyStructSequence_New(name_type_pytuple);  // new ref
  if (!nt_tuple) throw PyError();
  PyObject* stype = info(ParserLibrary::info(ptype).stype).py_stype().release();
  PyObject* cname = py::ostring(name).release();
  PyStructSequence_SetItem(nt_tuple, 0, cname);
  PyStructSequence_SetItem(nt_tuple, 1, stype);
  return py::oobj::from_new_reference(nt_tuple);
}

size_t Column::memory_footprint() const noexcept {
  return databuf.memory_footprint() +
         (strbuf? strbuf->size() : 0) + name.size() + sizeof(*this);
}



//---- ptype_iterator ----------------------------------------------------------

Column::ptype_iterator::ptype_iterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT Column::ptype_iterator::operator*() const {
  return curr_ptype;
}

RT Column::ptype_iterator::get_rtype() const {
  return rtype;
}

Column::ptype_iterator& Column::ptype_iterator::operator++() {
  if (curr_ptype < PT::Str32) {
    curr_ptype = static_cast<PT>(curr_ptype + 1);
  } else {
    *pqr = *pqr + 1;
  }
  return *this;
}

bool Column::ptype_iterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}


}  // namespace read
}  // namespace dt
