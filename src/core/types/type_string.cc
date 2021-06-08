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
#include "python/string.h"
#include "stype.h"
#include "types/type_invalid.h"
#include "types/type_string.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// Type_String
//------------------------------------------------------------------------------

bool Type_String::is_string() const {
  return true;
}

bool Type_String::can_be_read_as_cstring() const {
  return true;
}

TypeImpl* Type_String::common_type(TypeImpl* other) {
  if (other->is_string()) {
    auto stype1 = static_cast<int>(this->stype());
    auto stype2 = static_cast<int>(other->stype());
    return stype1 >= stype2? this : other;
  }
  if (other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}




//------------------------------------------------------------------------------
// Type_String32
//-----------------------------------------------------------------------------

Type_String32::Type_String32()
  : Type_String(SType::STR32) {}

std::string Type_String32::to_string() const {
  return "str32";
}




//------------------------------------------------------------------------------
// Type_String64
//------------------------------------------------------------------------------

Type_String64::Type_String64()
  : Type_String(SType::STR64) {}

std::string Type_String64::to_string() const {
  return "str64";
}




}  // namespace dt
