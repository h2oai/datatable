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
#include "frame/py_frame.h"
#include "stype.h"
#include "types/type_time.h"
#include "types/type_invalid.h"
namespace dt {



Type_Time64::Type_Time64() 
  : TypeImpl(SType::TIME64) {}


bool Type_Time64::can_be_read_as_int64() const { 
  return true; 
}

bool Type_Time64::is_time() const { 
  return true; 
}

std::string Type_Time64::to_string() const { 
  return "time64"; 
}

// The min/max values are rounded to microsecond resolution, because
// that's the resolution that python supports.
py::oobj Type_Time64::min() const {
  return py::odatetime(-9223285636854775000L);
}

py::oobj Type_Time64::max() const {
  return py::odatetime(9223372036854775000L);
}

const char* Type_Time64::struct_format() const { 
  return "q";  // Pretend this is int64
}

TypeImpl* Type_Time64::common_type(TypeImpl* other) {
  if (other->stype() == SType::TIME64 ||
      other->stype() == SType::DATE32 ||
      other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}




}  // namespace dt
