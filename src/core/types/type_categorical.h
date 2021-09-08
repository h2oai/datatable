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
#ifndef dt_TYPES_TYPE_CATEGORICAL_h
#define dt_TYPES_TYPE_CATEGORICAL_h

#include "column.h"
#include "types/typeimpl.h"

namespace dt {


class Type_Cat : public TypeImpl {
  private:
    template <typename T>
    void cast_obj_column_(Column& col) const;

  protected:
    Type elementType_;

  public:
    Type_Cat(SType, Type);
    bool is_categorical() const override;
    bool is_compound() const override;

    bool can_be_read_as_int8() const override;
    bool can_be_read_as_int16() const override;
    bool can_be_read_as_int32() const override;
    bool can_be_read_as_int64() const override;
    bool can_be_read_as_float32() const override;
    bool can_be_read_as_float64() const override;
    bool can_be_read_as_cstring() const override;
    bool can_be_read_as_pyobject() const override;
    bool can_be_read_as_column() const override;

    bool equals(const TypeImpl* other) const override;
    size_t hash() const noexcept override;
    TypeImpl* common_type(TypeImpl* other) override;
    Column cast_column(Column&& col) const override;
};


class Type_Cat8 : public Type_Cat {
  public:
    Type_Cat8(Type t);
    std::string to_string() const override;
};


class Type_Cat16 : public Type_Cat {
  public:
    Type_Cat16(Type t);
    std::string to_string() const override;
};


class Type_Cat32 : public Type_Cat {
  public:
    Type_Cat32(Type t);
    std::string to_string() const override;
};


}  // namespace dt
#endif
