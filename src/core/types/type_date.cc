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
#include "types/type_date.h"
#include "types/type_invalid.h"
namespace dt {



Type_Date32::Type_Date32() 
  : TypeImpl(SType::DATE32) {}


bool Type_Date32::can_be_read_as_int32() const { 
  return true; 
}

bool Type_Date32::is_time() const { 
  return true; 
}

std::string Type_Date32::to_string() const { 
  return "date32"; 
}

py::oobj Type_Date32::min() const {
  return py::odate(-std::numeric_limits<int>::max());
}

py::oobj Type_Date32::max() const {
  return py::odate(std::numeric_limits<int>::max() - 719468);
}

const char* Type_Date32::struct_format() const { 
  return "i";  // Pretend this is int32
}


TypeImpl* Type_Date32::common_type(TypeImpl* other) {
  if (other->stype() == SType::DATE32 || other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}




}  // namespace dt
