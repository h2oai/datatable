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
#ifndef dt_TYPES_TYPE_h
#define dt_TYPES_TYPE_h
#include "_dt.h"
#include "utils/tests.h"
namespace dt {

class TypeImpl;


/**
  * This class encompasses the type of a single column. It has a
  * "shared pointer" semantics so that it is easy to copy and pass
  * it around.
  *
  * Originally, the type of a column was governed by a simple enum
  * SType. However, eventually we came to the point where this is no
  * longer sufficient: certain types must carry additional
  * information that cannot be enumerated.
  *
  * The SType enum currently remains as a fallback; it may be
  * eliminated in the future.
  */
class Type {
  private:
    TypeImpl* impl_;   // owned

  public:
    //--------------------------------------------------------------------------
    // Constructors
    //--------------------------------------------------------------------------
    Type() noexcept;
    Type(const Type& other) noexcept;
    Type(Type&& other) noexcept;
    Type& operator=(const Type& other);
    Type& operator=(Type&& other);
    ~Type();

    static Type arr32(Type);
    static Type arr64(Type);
    static Type bool8();
    static Type cat8(Type);
    static Type cat16(Type);
    static Type cat32(Type);
    static Type date32();
    static Type float32();
    static Type float64();
    static Type int16();
    static Type int32();
    static Type int64();
    static Type int8();
    static Type obj64();
    static Type str32();
    static Type str64();
    static Type time64();
    static Type void0();

    static Type from_stype(SType);


    //--------------------------------------------------------------------------
    // Properties
    //--------------------------------------------------------------------------
    size_t hash() const;
    py::oobj min() const;
    py::oobj max() const;
    const char* struct_format() const;

    SType stype() const;
    bool is_array() const;
    bool is_boolean() const;
    bool is_boolean_or_void() const;
    bool is_categorical() const;
    bool is_compound() const;
    bool is_float() const;
    bool is_integer() const;
    bool is_integer_or_void() const;
    bool is_invalid() const;
    bool is_numeric() const;
    bool is_numeric_or_void() const;
    bool is_object() const;
    bool is_string() const;
    bool is_temporal() const;
    bool is_void() const;

    template<typename T>
    bool can_be_read_as() const;

    bool operator==(const Type& other) const;
    operator bool() const;
    std::string to_string() const;

    Type child() const;

    // (Optionally) change the current type so that it becomes
    // compatible with the type `other`. This can be used, for
    // example, when two columns of different types are passed to a
    // binary function; or when multiple columns need to be merged
    // into a single one; etc.
    //
    // If the current type is incompatible with `other`, then it will
    // be promoted into `Type::invalid()`.
    //
    void promote(const Type& other);

    static Type common(const Type&, const Type&);

    Column cast_column(Column&& column) const;

  private:
    friend class TypeImpl;
    Type(TypeImpl*&&) noexcept;
};


template<> bool Type::can_be_read_as<int8_t>() const;
template<> bool Type::can_be_read_as<int16_t>() const;
template<> bool Type::can_be_read_as<int32_t>() const;
template<> bool Type::can_be_read_as<int64_t>() const;
template<> bool Type::can_be_read_as<float>() const;
template<> bool Type::can_be_read_as<double>() const;
template<> bool Type::can_be_read_as<CString>() const;
template<> bool Type::can_be_read_as<py::oobj>() const;
template<> bool Type::can_be_read_as<Column>() const;



}  // namespace dt
#endif
