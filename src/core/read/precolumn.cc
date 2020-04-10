//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "python/string.h"
#include "read/precolumn.h"
namespace dt {
namespace read {



//---- Constructors ------------------------------------------------------------

PreColumn::PreColumn() {
  ptype_ = PT::Mu;
  rtype_ = RT::RAuto;
  type_bumped_ = false;
  present_in_output_ = true;
  present_in_buffer_ = true;
}

PreColumn::PreColumn(PreColumn&& o)
  : name_(std::move(o.name_)),
    databuf_(std::move(o.databuf_)),
    strbuf_(std::move(o.strbuf_)),
    ptype_(o.ptype_),
    rtype_(o.rtype_),
    type_bumped_(o.type_bumped_),
    present_in_output_(o.present_in_output_),
    present_in_buffer_(o.present_in_buffer_) {}

PreColumn::~PreColumn() {}



//---- Column's data -----------------------------------------------------------

void PreColumn::allocate(size_t nrows) {
  if (!present_in_output_) return;
  bool col_is_string = is_string();
  size_t allocsize = (nrows + col_is_string) * elemsize();
  databuf_.resize(allocsize);
  if (col_is_string) {
    if (elemsize() == 4)
      databuf_.set_element<int32_t>(0, 0);
    else
      databuf_.set_element<int64_t>(0, 0);
    if (!strbuf_) {
      strbuf_ = std::unique_ptr<MemoryWritableBuffer>(
                    new MemoryWritableBuffer(allocsize));
    }
  }
}

void* PreColumn::data_w() {
  return databuf_.wptr();
}

WritableBuffer* PreColumn::strdata_w() {
  return strbuf_.get();
}

Buffer PreColumn::extract_databuf() {
  return std::move(databuf_);
}

Buffer PreColumn::extract_strbuf() {
  if (!(strbuf_ && is_string())) return Buffer();
  strbuf_->finalize();
  return strbuf_->get_mbuf();
}



//---- Column's name -----------------------------------------------------------

const std::string& PreColumn::get_name() const noexcept {
  return name_;
}

void PreColumn::set_name(std::string&& newname) noexcept {
  name_ = std::move(newname);
}

void PreColumn::swap_names(PreColumn& other) noexcept {
  name_.swap(other.name_);
}

const char* PreColumn::repr_name(const GenericReader& g) const {
  const char* start = name_.c_str();
  const char* end = start + name_.size();
  return g.repr_binary(start, end, 25);
}



//---- Column's type -----------------------------------------------------------

PT PreColumn::get_ptype() const noexcept {
  return ptype_;
}

RT PreColumn::get_rtype() const noexcept {
  return rtype_;
}

SType PreColumn::get_stype() const {
  return ParserLibrary::info(ptype_).stype;
}

PreColumn::ptype_iterator
PreColumn::get_ptype_iterator(int8_t* qr_ptr) const {
  return PreColumn::ptype_iterator(ptype_, rtype_, qr_ptr);
}

void PreColumn::set_ptype(const PreColumn::ptype_iterator& it) {
  xassert(rtype_ == it.get_rtype());
  ptype_ = *it;
  type_bumped_ = true;
}

// Set .ptype_ to the provided value, disregarding the restrictions imposed
// by the .rtype_ field.
void PreColumn::force_ptype(PT new_ptype) {
  ptype_ = new_ptype;
}

void PreColumn::set_rtype(int64_t it) {
  rtype_ = static_cast<RT>(it);
  // Temporary
  switch (rtype_) {
    case RDrop:
      ptype_ = PT::Str32;
      present_in_output_ = false;
      present_in_buffer_ = false;
      break;
    case RAuto:    break;
    case RBool:    ptype_ = PT::Bool01; break;
    case RInt:     ptype_ = PT::Int32; break;
    case RInt32:   ptype_ = PT::Int32; break;
    case RInt64:   ptype_ = PT::Int64; break;
    case RFloat:   ptype_ = PT::Float32Hex; break;
    case RFloat32: ptype_ = PT::Float32Hex; break;
    case RFloat64: ptype_ = PT::Float64Plain; break;
    case RStr:     ptype_ = PT::Str32; break;
    case RStr32:   ptype_ = PT::Str32; break;
    case RStr64:   ptype_ = PT::Str64; break;
  }
}

const char* PreColumn::typeName() const {
  return ParserLibrary::info(ptype_).name.data();
}



//---- Column info -------------------------------------------------------------

bool PreColumn::is_string() const {
  return ParserLibrary::info(ptype_).isstring();
}

bool PreColumn::is_dropped() const {
  return rtype_ == RT::RDrop;
}

bool PreColumn::is_type_bumped() const {
  return type_bumped_;
}

bool PreColumn::is_in_output() const {
  return present_in_output_;
}

bool PreColumn::is_in_buffer() const {
  return present_in_buffer_;
}

size_t PreColumn::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(ptype_).elemsize);
}

void PreColumn::reset_type_bumped() {
  type_bumped_ = false;
}

void PreColumn::set_in_buffer(bool f) {
  present_in_buffer_ = f;
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

py::oobj PreColumn::py_descriptor() const {
  static PyTypeObject* name_type_pytuple = init_nametypepytuple();
  PyObject* nt_tuple = PyStructSequence_New(name_type_pytuple);  // new ref
  if (!nt_tuple) throw PyError();
  PyObject* stype = info(ParserLibrary::info(ptype_).stype).py_stype().release();
  PyObject* cname = py::ostring(name_).release();
  PyStructSequence_SetItem(nt_tuple, 0, cname);
  PyStructSequence_SetItem(nt_tuple, 1, stype);
  return py::oobj::from_new_reference(nt_tuple);
}

size_t PreColumn::memory_footprint() const noexcept {
  return databuf_.memory_footprint() +
         (strbuf_? strbuf_->size() : 0) + name_.size() + sizeof(*this);
}



//---- ptype_iterator ----------------------------------------------------------

PreColumn::ptype_iterator::ptype_iterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT PreColumn::ptype_iterator::operator*() const {
  return curr_ptype;
}

RT PreColumn::ptype_iterator::get_rtype() const {
  return rtype;
}

PreColumn::ptype_iterator& PreColumn::ptype_iterator::operator++() {
  if (curr_ptype < PT::Str32) {
    curr_ptype = static_cast<PT>(curr_ptype + 1);
  } else {
    *pqr = *pqr + 1;
  }
  return *this;
}

bool PreColumn::ptype_iterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}




}}  // namespace dt::read
