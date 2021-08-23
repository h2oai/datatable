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
#ifndef dt_TYPES_TYPE_ARRAY_h
#define dt_TYPES_TYPE_ARRAY_h
#include "types/typeimpl.h"
namespace dt {


class Type_Array : public TypeImpl {
  protected:
    Type childType_;

  public:
    Type_Array(Type t, SType stype);
    bool is_array() const override;
    bool is_compound() const override;
    bool can_be_read_as_column() const override;
    bool equals(const TypeImpl* other) const override;
    size_t hash() const noexcept override;
    TypeImpl* common_type(TypeImpl* other) override;
    // Column cast_column(Column&& col) const override;
    Type child_type() const override;
};



class Type_Arr32 : public Type_Array {
  public:
    Type_Arr32(Type t);
    std::string to_string() const override;
};



class Type_Arr64 : public Type_Array {
  public:
    Type_Arr64(Type t);
    std::string to_string() const override;
};




}  // namespace dt
#endif
