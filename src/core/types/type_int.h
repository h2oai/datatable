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
#include "types/type_impl.h"
#include "utils/assert.h"
namespace dt {



class Type_Int : public TypeImpl {
  protected:
    using TypeImpl::TypeImpl;

  public:
    bool is_integer() const override { return true; }
    bool is_numeric() const override { return true; }
};


class Type_Int8 : public Type_Int {
  public:
    Type_Int8() : Type_Int(SType::INT8) {}
    bool can_be_read_as_int8()  const override { return true; }
    std::string to_string() const override { return "int8"; }
};


class Type_Int16 : public Type_Int {
  public:
    Type_Int16() : Type_Int(SType::INT16) {}
    bool can_be_read_as_int16()  const override { return true; }
    std::string to_string() const override { return "int16"; }
};


class Type_Int32 : public Type_Int {
  public:
    Type_Int32() : Type_Int(SType::INT32) {}
    bool can_be_read_as_int32()  const override { return true; }
    std::string to_string() const override { return "int32"; }
};


class Type_Int64 : public Type_Int {
  public:
    Type_Int64() : Type_Int(SType::INT64) {}
    bool can_be_read_as_int64()  const override { return true; }
    std::string to_string() const override { return "int64"; }
};




}  // namespace dt
#endif
