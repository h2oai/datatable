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
#ifndef dt_TYPES_TYPE_IMPL_h
#define dt_TYPES_TYPE_IMPL_h
#include "types/type.h"
namespace dt {


class TypeImpl {
  private:
    SType stype_;
    int : 24;
    int32_t refcount_;
    friend class Type;

    void acquire() noexcept;
    void release() noexcept;

  public:
    TypeImpl();
    virtual ~TypeImpl();

    SType stype() const { return stype_; }
    virtual size_t hash() const noexcept;
    virtual py::oobj min() const;
    virtual py::oobj max() const;
    virtual const char* struct_format() const;
    virtual TypeImpl* common_type(TypeImpl* other) = 0;

    virtual bool is_array() const;
    virtual bool is_boolean() const;
    virtual bool is_categorical() const;
    virtual bool is_compound() const;
    virtual bool is_float() const;
    virtual bool is_integer() const;
    virtual bool is_invalid() const;
    virtual bool is_numeric() const;
    virtual bool is_object() const;
    virtual bool is_string() const;
    virtual bool is_temporal() const;
    virtual bool is_void() const;

    virtual bool can_be_read_as_int8() const;
    virtual bool can_be_read_as_int16() const;
    virtual bool can_be_read_as_int32() const;
    virtual bool can_be_read_as_int64() const;
    virtual bool can_be_read_as_float32() const;
    virtual bool can_be_read_as_float64() const;
    virtual bool can_be_read_as_cstring() const;
    virtual bool can_be_read_as_pyobject() const;
    virtual bool can_be_read_as_column() const;

    // Default implementation only checks the equality of stypes.
    // Override if your TypeImpl contains more information.
    virtual bool equals(const TypeImpl* other) const;

    // This method must be implemented by each subclass
    virtual std::string to_string() const = 0;

    // Type-casting functionality: cast column `col` into the current type.
    // The new type-cast column of `this` type is returned.
    virtual Column cast_column(Column&& col) const;

  protected:
    TypeImpl(SType stype);
};




}  // namespace dt
#endif
