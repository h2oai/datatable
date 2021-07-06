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
#include "types/type_list.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// Type_List32
//------------------------------------------------------------------------------

Type_List32::Type_List32(Type t)
  : TypeImpl(SType::LIST32),
    elementType_(t) {}

bool Type_List32::is_compound() const { 
  return true; 
}

bool Type_List32::is_list() const {
  return true;
}

std::string Type_List32::to_string() const {
  return "list32[" + elementType_.to_string() + "]";
}

TypeImpl* Type_List32::common_type(TypeImpl* other) {
  if (other->is_list()) {
    return other->stype() > stype() ? other : this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}



//------------------------------------------------------------------------------
// Type_List64
//------------------------------------------------------------------------------

Type_List64::Type_List64(Type t)
  : TypeImpl(SType::LIST32),
    elementType_(t) {}

bool Type_List64::is_compound() const { 
  return true; 
}

bool Type_List64::is_list() const {
  return true;
}

std::string Type_List64::to_string() const {
  return "list64[" + elementType_.to_string() + "]";
}

TypeImpl* Type_List64::common_type(TypeImpl* other) {
  if (other->is_list()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}




}  // namespace dt
