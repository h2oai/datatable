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
#include <limits>
#include "python/int.h"
#include "stype.h"
#include "types/type_int.h"
#include "utils/assert.h"
namespace dt {


//------------------------------------------------------------------------------
// Type_Int8
//------------------------------------------------------------------------------

Type_Int8::Type_Int8() 
  : TypeImpl_Numeric(SType::INT8) {}

bool Type_Int8::is_integer() const { 
  return true; 
}

bool Type_Int8::can_be_read_as_int8() const {
  return true; 
}

std::string Type_Int8::to_string() const { 
  return "int8"; 
}

py::oobj Type_Int8::min() const { 
  return py::oint(-127); 
}

py::oobj Type_Int8::max() const { 
  return py::oint(127); 
}

const char* Type_Int8::struct_format() const { 
  return "b"; 
}




//------------------------------------------------------------------------------
// Type_Int16
//------------------------------------------------------------------------------

Type_Int16::Type_Int16()
  : TypeImpl_Numeric(SType::INT16) {}

bool Type_Int16::is_integer() const { 
  return true; 
}

bool Type_Int16::can_be_read_as_int16() const {
  return true; 
}

std::string Type_Int16::to_string() const {
  return "int16";
}

py::oobj Type_Int16::min() const {
  return py::oint(-32767);
}

py::oobj Type_Int16::max() const {
  return py::oint(32767);
}

const char* Type_Int16::struct_format() const {
  return "h";
}




//------------------------------------------------------------------------------
// Type_Int32
//------------------------------------------------------------------------------

Type_Int32::Type_Int32()
  : TypeImpl_Numeric(SType::INT32) {}

bool Type_Int32::is_integer() const { 
  return true; 
}

bool Type_Int32::can_be_read_as_int32() const {
  return true; 
}

std::string Type_Int32::to_string() const {
  return "int32";
}

py::oobj Type_Int32::min() const {
  return py::oint(-std::numeric_limits<int32_t>::max());
}

py::oobj Type_Int32::max() const {
  return py::oint(std::numeric_limits<int32_t>::max());
}

const char* Type_Int32::struct_format() const {
  return "i";
}




//------------------------------------------------------------------------------
// Type_Int64
//------------------------------------------------------------------------------

Type_Int64::Type_Int64()
  : TypeImpl_Numeric(SType::INT64) {}

bool Type_Int64::is_integer() const { 
  return true; 
}

bool Type_Int64::can_be_read_as_int64() const {
  return true; 
}

std::string Type_Int64::to_string() const {
  return "int64";
}

py::oobj Type_Int64::min() const {
  return py::oint(-std::numeric_limits<int64_t>::max());
}

py::oobj Type_Int64::max() const {
  return py::oint(std::numeric_limits<int64_t>::max());
}

const char* Type_Int64::struct_format() const {
  return "q";
}




}  // namespace dt
