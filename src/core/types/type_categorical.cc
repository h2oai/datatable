//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "stype.h"
#include "types/type_invalid.h"
#include "types/type_categorical.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// Type_Cat
//------------------------------------------------------------------------------

Type_Cat::Type_Cat(SType st, Type t) : TypeImpl(st) {
  if (t.is_categorical()) {
    throw TypeError()
      << "Categories are not allowed to be of a categorical type";
  }
  elementType_ = std::move(t);
}

bool Type_Cat::is_compound() const {
  return true;
}

bool Type_Cat::is_categorical() const {
  return true;
}

bool Type_Cat::can_be_read_as_int8() const {
  return elementType_.can_be_read_as<int8_t>();
}

bool Type_Cat::can_be_read_as_int16() const {
  return elementType_.can_be_read_as<int16_t>();
}

bool Type_Cat::can_be_read_as_int32() const {
  return elementType_.can_be_read_as<int32_t>();
}

bool Type_Cat::can_be_read_as_int64() const {
  return elementType_.can_be_read_as<int64_t>();
}

bool Type_Cat::can_be_read_as_float32() const {
  return elementType_.can_be_read_as<float>();
}

bool Type_Cat::can_be_read_as_float64() const {
  return elementType_.can_be_read_as<double>();
}

bool Type_Cat::can_be_read_as_cstring() const {
  return elementType_.can_be_read_as<CString>();
}

bool Type_Cat::can_be_read_as_pyobject() const {
  return elementType_.can_be_read_as<py::oobj>();
}

bool Type_Cat::can_be_read_as_column() const {
  return elementType_.can_be_read_as<Column>();
}


TypeImpl* Type_Cat::common_type(TypeImpl* other) {
  if (other->is_categorical() &&
      elementType_ == reinterpret_cast<const Type_Cat*>(other)->elementType_)
  {
    return other->stype() > stype() ? other : this;
  }
  if (other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


bool Type_Cat::equals(const TypeImpl* other) const {
  return other->stype() == stype() &&
         elementType_ == reinterpret_cast<const Type_Cat*>(other)->elementType_;
}


size_t Type_Cat::hash() const noexcept {
  return static_cast<size_t>(stype()) + STYPES_COUNT * elementType_.hash();
}


//------------------------------------------------------------------------------
// Type_Cat8
//------------------------------------------------------------------------------

Type_Cat8::Type_Cat8(Type t) : Type_Cat(SType::CAT8, t) {}

std::string Type_Cat8::to_string() const {
  return "cat8(" + elementType_.to_string() + ")";
}


//------------------------------------------------------------------------------
// Type_Cat16
//------------------------------------------------------------------------------

Type_Cat16::Type_Cat16(Type t) : Type_Cat(SType::CAT16, t) {}

std::string Type_Cat16::to_string() const {
  return "cat16(" + elementType_.to_string() + ")";
}

//------------------------------------------------------------------------------
// Type_Cat32
//------------------------------------------------------------------------------

Type_Cat32::Type_Cat32(Type t) : Type_Cat(SType::CAT32, std::move(t)) {}

std::string Type_Cat32::to_string() const {
  return "cat32(" + elementType_.to_string() + ")";
}


}  // namespace dt
