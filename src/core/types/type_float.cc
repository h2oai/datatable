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
#include "python/float.h"
#include "stype.h"
#include "types/type_float.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// Type_Float32
//------------------------------------------------------------------------------

Type_Float32::Type_Float32()
  : TypeImpl_Numeric(SType::FLOAT32) {}


bool Type_Float32::is_float() const {
  return true;
}

bool Type_Float32::can_be_read_as_float32() const {
  return true;
}

std::string Type_Float32::to_string() const {
  return "float32";
}

py::oobj Type_Float32::min() const {
  return py::ofloat(-std::numeric_limits<float>::max());
}

py::oobj Type_Float32::max() const {
  return py::ofloat(std::numeric_limits<float>::max());
}

const char* Type_Float32::struct_format() const {
  return "f";
}




//------------------------------------------------------------------------------
// Type_Float64
//------------------------------------------------------------------------------

Type_Float64::Type_Float64()
  : TypeImpl_Numeric(SType::FLOAT64) {}


bool Type_Float64::is_float() const {
  return true;
}

bool Type_Float64::can_be_read_as_float64() const {
  return true;
}

std::string Type_Float64::to_string() const {
  return "float64";
}

py::oobj Type_Float64::min() const {
  return py::ofloat(-std::numeric_limits<double>::max());
}

py::oobj Type_Float64::max() const {
  return py::ofloat(std::numeric_limits<double>::max());
}

const char* Type_Float64::struct_format() const {
  return "d";
}




}  // namespace dt
