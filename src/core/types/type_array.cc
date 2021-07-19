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
#include "types/type_array.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// Type_Arr32
//------------------------------------------------------------------------------

Type_Arr32::Type_Arr32(Type t)
  : TypeImpl(SType::ARR32),
    elementType_(t) {}

bool Type_Arr32::is_compound() const {
  return true; 
}

bool Type_Arr32::is_array() const {
  return true;
}

std::string Type_Arr32::to_string() const {
  return "arr32(" + elementType_.to_string() + ")";
}


TypeImpl* Type_Arr32::common_type(TypeImpl* other) {
  if (other->is_array()) {
    return other->stype() > stype() ? other : this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


bool Type_Arr32::equals(const TypeImpl* other) const {
  return other->stype() == stype() &&
         elementType_ == reinterpret_cast<const Type_Arr32*>(other)->elementType_;
}

size_t Type_Arr32::hash() const noexcept {
  return static_cast<size_t>(stype()) + STYPES_COUNT * elementType_.hash();
}




//------------------------------------------------------------------------------
// Type_Arr64
//------------------------------------------------------------------------------

Type_Arr64::Type_Arr64(Type t)
  : TypeImpl(SType::ARR64),
    elementType_(t) {}

bool Type_Arr64::is_compound() const {
  return true; 
}

bool Type_Arr64::is_array() const {
  return true;
}

std::string Type_Arr64::to_string() const {
  return "arr64(" + elementType_.to_string() + ")";
}


TypeImpl* Type_Arr64::common_type(TypeImpl* other) {
  if (other->is_array()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


bool Type_Arr64::equals(const TypeImpl* other) const {
  return other->stype() == stype() &&
         elementType_ == reinterpret_cast<const Type_Arr64*>(other)->elementType_;
}

size_t Type_Arr64::hash() const noexcept {
  return static_cast<size_t>(stype()) + STYPES_COUNT * elementType_.hash();
}




}  // namespace dt
