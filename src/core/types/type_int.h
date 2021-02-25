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
#ifndef dt_TYPES_TYPE_INT_h
#define dt_TYPES_TYPE_INT_h
#include <limits>
#include "python/int.h"
#include "types/type_numeric.h"
#include "utils/assert.h"
namespace dt {



class Type_Int8 : public Type_Numeric {
  public:
    Type_Int8() : Type_Numeric(SType::INT8) {}
    bool is_integer() const override { return true; }
    bool can_be_read_as_int8()  const override { return true; }
    std::string to_string() const override { return "int8"; }
    py::oobj min() const override { return py::oint(-127); }
    py::oobj max() const override { return py::oint(127); }
};


class Type_Int16 : public Type_Numeric {
  public:
    Type_Int16() : Type_Numeric(SType::INT16) {}
    bool is_integer() const override { return true; }
    bool can_be_read_as_int16()  const override { return true; }
    std::string to_string() const override { return "int16"; }
    py::oobj min() const override { return py::oint(-32767); }
    py::oobj max() const override { return py::oint(32767); }
};


class Type_Int32 : public Type_Numeric {
  public:
    Type_Int32() : Type_Numeric(SType::INT32) {}
    bool is_integer() const override { return true; }
    bool can_be_read_as_int32()  const override { return true; }
    std::string to_string() const override { return "int32"; }

    py::oobj min() const override {
      return py::oint(-std::numeric_limits<int32_t>::max());
    }
    py::oobj max() const override {
      return py::oint(std::numeric_limits<int32_t>::max());
    }
};


class Type_Int64 : public Type_Numeric {
  public:
    Type_Int64() : Type_Numeric(SType::INT64) {}
    bool is_integer() const override { return true; }
    bool can_be_read_as_int64()  const override { return true; }
    std::string to_string() const override { return "int64"; }

    py::oobj min() const override {
      return py::oint(-std::numeric_limits<int64_t>::max());
    }
    py::oobj max() const override {
      return py::oint(std::numeric_limits<int64_t>::max());
    }
};




}  // namespace dt
#endif
