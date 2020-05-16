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
#include "read/output_column.h"
#include "read/input_column.h"
#include "utils/temporary_file.h"
#include "column.h"
namespace dt {
namespace read {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

InputColumn::InputColumn()
  : parse_type_(PT::Mu),
    requested_type_(RT::RAuto),
    type_bumped_(false),
    present_in_output_(true),
    present_in_buffer_(true) {}

InputColumn::InputColumn(InputColumn&& o) noexcept
  : name_(std::move(o.name_)),
    parse_type_(o.parse_type_),
    requested_type_(o.requested_type_),
    type_bumped_(o.type_bumped_),
    present_in_output_(o.present_in_output_),
    present_in_buffer_(o.present_in_buffer_),
    outcol_(std::move(o.outcol_)) {}



OutputColumn& InputColumn::outcol() {
  return outcol_;
}




//---- Column's name -----------------------------------------------------------

const std::string& InputColumn::get_name() const noexcept {
  return name_;
}

void InputColumn::set_name(std::string&& newname) noexcept {
  name_ = std::move(newname);
}

void InputColumn::swap_names(InputColumn& other) noexcept {
  name_.swap(other.name_);
}

const char* InputColumn::repr_name(const GenericReader& g) const {
  const char* start = name_.c_str();
  const char* end = start + name_.size();
  return g.repr_binary(start, end, 25);
}



//---- Column's type -----------------------------------------------------------

PT InputColumn::get_ptype() const noexcept {
  return parse_type_;
}

RT InputColumn::get_rtype() const noexcept {
  return requested_type_;
}

SType InputColumn::get_stype() const {
  return ParserLibrary::info(parse_type_).stype;
}

InputColumn::ptype_iterator
InputColumn::get_ptype_iterator(int8_t* qr_ptr) const {
  return InputColumn::ptype_iterator(parse_type_, requested_type_, qr_ptr);
}

void InputColumn::set_ptype(PT new_ptype) {
  type_bumped_ = true;
  parse_type_ = new_ptype;
  outcol_.set_stype(get_stype());
  outcol_.type_bumped_ = true;
}

// Set .parse_type_ to the provided value, disregarding the restrictions imposed
// by the .requested_type_ field.
void InputColumn::force_ptype(PT new_ptype) {
  parse_type_ = new_ptype;
  outcol_.set_stype(get_stype());
}

void InputColumn::set_rtype(int64_t it) {
  requested_type_ = static_cast<RT>(it);
  // Temporary
  switch (requested_type_) {
    case RDrop:
      parse_type_ = PT::Str32;
      present_in_output_ = false;
      break;
    case RAuto:    break;
    case RBool:    parse_type_ = PT::Bool01; break;
    case RInt:     parse_type_ = PT::Int32; break;
    case RInt32:   parse_type_ = PT::Int32; break;
    case RInt64:   parse_type_ = PT::Int64; break;
    case RFloat:   parse_type_ = PT::Float32Hex; break;
    case RFloat32: parse_type_ = PT::Float32Hex; break;
    case RFloat64: parse_type_ = PT::Float64Plain; break;
    case RStr:     parse_type_ = PT::Str32; break;
    case RStr32:   parse_type_ = PT::Str32; break;
    case RStr64:   parse_type_ = PT::Str64; break;
  }
  outcol_.set_stype(get_stype());
}

const char* InputColumn::typeName() const {
  return ParserLibrary::info(parse_type_).name.data();
}



//---- Column info -------------------------------------------------------------

bool InputColumn::is_string() const {
  return ParserLibrary::info(parse_type_).isstring();
}

bool InputColumn::is_dropped() const noexcept {
  return requested_type_ == RT::RDrop;
}

bool InputColumn::is_type_bumped() const noexcept {
  return type_bumped_;
}

bool InputColumn::is_in_output() const noexcept {
  return present_in_output_;
}

bool InputColumn::is_in_buffer() const noexcept {
  return present_in_buffer_;
}

size_t InputColumn::elemsize() const {
  return static_cast<size_t>(ParserLibrary::info(parse_type_).elemsize);
}

void InputColumn::reset_type_bumped() {
  type_bumped_ = false;
  outcol_.type_bumped_ = false;
}

size_t InputColumn::nrows_archived() const noexcept {
  return outcol_.nrows_in_chunks_;
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


py::oobj InputColumn::py_descriptor() const {
  static PyTypeObject* name_type_pytuple = init_nametypepytuple();
  PyObject* nt_tuple = PyStructSequence_New(name_type_pytuple);  // new ref
  if (!nt_tuple) throw PyError();
  PyObject* stype = info(ParserLibrary::info(parse_type_).stype).py_stype().release();
  PyObject* cname = py::ostring(name_).release();
  PyStructSequence_SetItem(nt_tuple, 0, cname);
  PyStructSequence_SetItem(nt_tuple, 1, stype);
  return py::oobj::from_new_reference(nt_tuple);
}


size_t InputColumn::memory_footprint() const {
  size_t sz = archived_size();
  sz += outcol_.databuf_.memory_footprint();
  sz += outcol_.strbuf_? outcol_.strbuf_->size() : 0;
  sz += name_.size() + sizeof(*this);
  return sz;
}


size_t InputColumn::archived_size() const {
  size_t sz = 0;
  for (const auto& col : outcol_.chunks_) {
    size_t k = col.get_num_data_buffers();
    for (size_t i = 0; i < k; ++i) {
      sz += col.get_data_size(i);
    }
  }
  return sz;
}


void InputColumn::prepare_for_rereading() {
  if (type_bumped_ && present_in_output_) {
    present_in_buffer_ = true;
    type_bumped_ = false;
    outcol_.chunks_.clear();
    outcol_.nrows_in_chunks_ = 0;
    outcol_.strbuf_ = nullptr;
    outcol_.type_bumped_ = false;
    outcol_.present_in_buffer_ = true;
  }
  else {
    present_in_buffer_ = false;
    outcol_.present_in_buffer_ = false;
  }
}



//---- ptype_iterator ----------------------------------------------------------

InputColumn::ptype_iterator::ptype_iterator(PT pt, RT rt, int8_t* qr_ptr)
  : pqr(qr_ptr), rtype(rt), orig_ptype(pt), curr_ptype(pt) {}

PT InputColumn::ptype_iterator::operator*() const {
  return curr_ptype;
}

RT InputColumn::ptype_iterator::get_rtype() const {
  return rtype;
}

InputColumn::ptype_iterator& InputColumn::ptype_iterator::operator++() {
  if (curr_ptype < PT::Str32) {
    curr_ptype = static_cast<PT>(curr_ptype + 1);
  } else {
    *pqr = *pqr + 1;
  }
  return *this;
}

bool InputColumn::ptype_iterator::has_incremented() const {
  return curr_ptype != orig_ptype;
}





}}  // namespace dt::read
