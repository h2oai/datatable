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
#include "column.h"
#include "csv/reader.h"
#include "python/string.h"
#include "read/input_column.h"
#include "read/output_column.h"
#include "read/parsers/info.h"
#include "read/parsers/rt.h"
#include "stype.h"
#include "utils/temporary_file.h"
namespace dt {
namespace read {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

InputColumn::InputColumn()
  : parse_type_(PT::Void),
    requested_type_(RT::RAuto) {}


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
  return parser_infos[parse_type_].type().stype();
}


void InputColumn::set_ptype(PT new_ptype) {
  parse_type_ = new_ptype;
}


void InputColumn::set_rtype(int64_t it) {
  requested_type_ = static_cast<RT>(it);
  // Temporary
  switch (requested_type_) {
    case RDrop:    parse_type_ = PT::Str32; break;
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
    // If at some point we implement creating str64 columns from fread directly,
    // then this line will have to be changed. For now, though, if the user
    // requests a str64 column type, we'll create a regular str32 instead.
    case RStr64:   parse_type_ = PT::Str32; break;
  }
}

const char* InputColumn::typeName() const {
  return parser_infos[parse_type_].name().data();
}



//---- Column info -------------------------------------------------------------

bool InputColumn::is_string() const {
  return parser_infos[parse_type_].type().is_string();
}

bool InputColumn::is_dropped() const noexcept {
  return requested_type_ == RT::RDrop;
}

size_t InputColumn::elemsize() const {
  return stype_elemsize(get_stype());
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
  PyObject* stype = stype_to_pyobj(get_stype()).release();
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




}}  // namespace dt::read
