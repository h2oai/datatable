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
#ifndef dt_TYPES_TYPE_FLOAT_h
#define dt_TYPES_TYPE_FLOAT_h
#include "types/typeimpl_numeric.h"
namespace dt {



class Type_Float32 : public TypeImpl_Numeric {
  public:
    Type_Float32();
    bool is_float() const override;
    bool can_be_read_as_float32() const override;
    std::string to_string() const override;

    py::oobj min() const override;
    py::oobj max() const override;
    const char* struct_format() const override;
};



class Type_Float64 : public TypeImpl_Numeric {
  public:
    Type_Float64();
    bool is_float() const override;
    bool can_be_read_as_float64() const override;
    std::string to_string() const override;

    py::oobj min() const override;
    py::oobj max() const override;
    const char* struct_format() const override;
};




}  // namespace dt
#endif
