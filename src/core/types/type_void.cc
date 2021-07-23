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
#include "column/const.h"
#include "stype.h"
#include "types/type_void.h"
namespace dt {



Type_Void::Type_Void() 
  : TypeImpl(SType::VOID) {}


bool Type_Void::is_boolean() const { return true; }
bool Type_Void::is_integer() const { return true; }
bool Type_Void::is_float()   const { return true; }
bool Type_Void::is_numeric() const { return true; }
bool Type_Void::is_void()    const { return true; }
bool Type_Void::can_be_read_as_int8() const    { return true; }
bool Type_Void::can_be_read_as_int16() const   { return true; }
bool Type_Void::can_be_read_as_int32() const   { return true; }
bool Type_Void::can_be_read_as_int64() const   { return true; }
bool Type_Void::can_be_read_as_float32() const { return true; }
bool Type_Void::can_be_read_as_float64() const { return true; }
bool Type_Void::can_be_read_as_date() const    { return true; }
bool Type_Void::can_be_read_as_cstring() const { return true; }


std::string Type_Void::to_string() const {
  return "void"; 
}


TypeImpl* Type_Void::common_type(TypeImpl* other) {
  return other;
}


const char* Type_Void::struct_format() const {
  return "V"; 
}


// We allow columns of any type to be "cast into void". Not sure why.
Column Type_Void::cast_column(Column&& col) const {
  return Column(new ConstNa_ColumnImpl(col.nrows(), SType::VOID));
}




}  // namespace dt
