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
#include "stype.h"
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
    Type() noexcept;
    Type(const Type& other) noexcept;
    Type(Type&& other) noexcept;
    Type& operator=(const Type& other);
    Type& operator=(Type&& other);
    ~Type();

    static Type void0();
    static Type bool8();
    static Type int8();
    static Type int16();
    static Type int32();
    static Type int64();
    static Type float32();
    static Type float64();
    static Type str32();
    static Type str64();
    static Type obj64();
    static Type from_stype(SType);

    SType stype() const;
    bool is_boolean() const;
    bool is_integer() const;
    bool is_float() const;
    bool is_numeric() const;
    bool is_string() const;
    bool is_object() const;

  private:
    Type(TypeImpl*&&) noexcept;
};





}  // namespace dt
#endif
