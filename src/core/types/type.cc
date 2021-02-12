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
#include "python/obj.h"
#include "stype.h"
#include "types/type.h"
#include "types/type_bool.h"
#include "types/type_float.h"
#include "types/type_impl.h"
#include "types/type_int.h"
#include "types/type_object.h"
#include "types/type_string.h"
#include "types/type_void.h"
namespace dt {


Type::Type(TypeImpl*&& impl) noexcept  // private
  : impl_(impl) {}

Type::Type() noexcept
  : impl_(nullptr) {}

Type::Type(const Type& other) noexcept {
  impl_ = other.impl_;
  if (impl_) impl_->acquire();
}

Type::Type(Type&& other) noexcept {
  impl_ = other.impl_;
  other.impl_ = nullptr;
}

Type& Type::operator=(const Type& other) {
  auto old_impl = impl_;
  impl_ = other.impl_;
  if (old_impl) impl_->release();
  if (impl_) impl_->acquire();
  return *this;
}

Type& Type::operator=(Type&& other) {
  auto old_impl = impl_;
  impl_ = other.impl_;
  other.impl_ = old_impl;
  return *this;
}

Type::~Type() {
  if (impl_) impl_->release();
}



Type Type::void0()   { return Type(new Type_Void); }
Type Type::bool8()   { return Type(new Type_Bool8); }
Type Type::int8()    { return Type(new Type_Int8); }
Type Type::int16()   { return Type(new Type_Int16); }
Type Type::int32()   { return Type(new Type_Int32); }
Type Type::int64()   { return Type(new Type_Int64); }
Type Type::float32() { return Type(new Type_Float32); }
Type Type::float64() { return Type(new Type_Float64); }
Type Type::str32()   { return Type(new Type_String32); }
Type Type::str64()   { return Type(new Type_String64); }
Type Type::obj64()   { return Type(new Type_Object); }

Type Type::from_stype(SType stype) {
  switch (stype) {
    case SType::VOID:    return void0();
    case SType::BOOL:    return bool8();
    case SType::INT8:    return int8();
    case SType::INT16:   return int16();
    case SType::INT32:   return int32();
    case SType::INT64:   return int64();
    case SType::FLOAT32: return float32();
    case SType::FLOAT64: return float64();
    case SType::STR32:   return str32();
    case SType::STR64:   return str64();
    case SType::OBJ:     return obj64();
    default: break;
  }
  throw NotImplError() << "Cannot instantiate Type from " << stype;
}


SType Type::stype() const { return impl_->stype_; }

bool Type::is_boolean() const { return impl_->is_boolean(); }
bool Type::is_integer() const { return impl_->is_integer(); }
bool Type::is_float()   const { return impl_->is_float(); }
bool Type::is_numeric() const { return impl_->is_numeric(); }
bool Type::is_string()  const { return impl_->is_string(); }
bool Type::is_object()  const { return impl_->is_object(); }


template<typename T> bool Type::can_be_read_as() const { return false; }
template<> bool Type::can_be_read_as<int8_t>()   const { return impl_->can_be_read_as_int8(); }
template<> bool Type::can_be_read_as<int16_t>()  const { return impl_->can_be_read_as_int16(); }
template<> bool Type::can_be_read_as<int32_t>()  const { return impl_->can_be_read_as_int32(); }
template<> bool Type::can_be_read_as<int64_t>()  const { return impl_->can_be_read_as_int64(); }
template<> bool Type::can_be_read_as<float>()    const { return impl_->can_be_read_as_float32(); }
template<> bool Type::can_be_read_as<double>()   const { return impl_->can_be_read_as_float64(); }
template<> bool Type::can_be_read_as<CString>()  const { return impl_->can_be_read_as_cstring(); }
template<> bool Type::can_be_read_as<py::oobj>() const { return impl_->can_be_read_as_pyobject(); }


bool Type::operator==(const Type& other) const {
  return (impl_ == other.impl_) || (impl_->equals(other.impl_));
}

std::string Type::to_string() const {
  if (!impl_) return "Type()";
  return impl_->to_string();
}



}  // namespace dt
