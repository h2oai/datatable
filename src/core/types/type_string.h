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
#ifndef dt_TYPES_TYPE_STRING_h
#define dt_TYPES_TYPE_STRING_h
#include "types/type_impl.h"
#include "utils/assert.h"
namespace dt {



class Type_String : public TypeImpl {
  protected:
    using TypeImpl::TypeImpl;

  public:
    bool is_string() const override { return true; }
    bool can_be_read_as_cstring() const override { return true; }
};


class Type_String32 : public Type_String {
  public:
    Type_String32() : Type_String(SType::STR32) {}
    std::string to_string() const override { return "str32"; }
};


class Type_String64 : public Type_String {
  public:
    Type_String64() : Type_String(SType::STR64) {}
    std::string to_string() const override { return "str64"; }
};




}  // namespace dt
#endif
