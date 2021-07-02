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
#include "column.h"
#include "python/obj.h"
#include "stype.h"
#include "types/type.h"
#include "types/type_bool.h"
#include "types/type_date.h"
#include "types/type_float.h"
#include "types/type_int.h"
#include "types/type_object.h"
#include "types/type_string.h"
#include "types/type_time.h"
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
  if (impl_ != other.impl_) {
    auto old_impl = impl_;
    impl_ = other.impl_;
    if (old_impl) old_impl->release();
    if (impl_) impl_->acquire();
  }
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



Type Type::bool8()   { return Type(new Type_Bool8); }
Type Type::date32()  { return Type(new Type_Date32); }
Type Type::float32() { return Type(new Type_Float32); }
Type Type::float64() { return Type(new Type_Float64); }
Type Type::int16()   { return Type(new Type_Int16); }
Type Type::int32()   { return Type(new Type_Int32); }
Type Type::int64()   { return Type(new Type_Int64); }
Type Type::int8()    { return Type(new Type_Int8); }
Type Type::obj64()   { return Type(new Type_Object); }
Type Type::str32()   { return Type(new Type_String32); }
Type Type::str64()   { return Type(new Type_String64); }
Type Type::time64()  { return Type(new Type_Time64); }
Type Type::void0()   { return Type(new Type_Void); }

Type Type::from_stype(SType stype) {
  switch (stype) {
    case SType::AUTO:    return Type();
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
    case SType::DATE32:  return date32();
    case SType::TIME64:  return time64();
    case SType::OBJ:     return obj64();
    default: break;
  }
  throw NotImplError() << "Cannot instantiate Type from " << stype;
}

size_t Type::hash() const { return impl_->hash(); }
py::oobj Type::min() const { return impl_->min(); }
py::oobj Type::max() const { return impl_->max(); }
SType Type::stype() const { return impl_->stype_; }
const char* Type::struct_format() const { return impl_->struct_format(); }

void Type::promote(const Type& other) {
  if (impl_) {
    if (!other.impl_) return;
    TypeImpl* res = impl_->common_type(other.impl_);
    if (res != impl_) {
      impl_->release();
      impl_ = res;
      if (res == other.impl_) res->acquire();
    }
  }
  else {
    impl_ = other.impl_;
    if (impl_) impl_->acquire();
  }
}

Type Type::common(const Type& type1, const Type& type2) {
  if (!type1.impl_) return type2;
  TypeImpl* res = type1.impl_->common_type(type2.impl_);
  if (res == type1.impl_) return type1;
  if (res == type2.impl_) return type2;
  res->acquire();
  return Type(std::move(res));
}


bool Type::is_void()     const { return impl_ && impl_->stype_ == SType::VOID; }
bool Type::is_invalid()  const { return impl_ && impl_->is_invalid(); }
bool Type::is_boolean()  const { return impl_ && impl_->is_boolean(); }
bool Type::is_integer()  const { return impl_ && impl_->is_integer(); }
bool Type::is_float()    const { return impl_ && impl_->is_float(); }
bool Type::is_numeric()  const { return impl_ && impl_->is_numeric(); }
bool Type::is_string()   const { return impl_ && impl_->is_string(); }
bool Type::is_object()   const { return impl_ && impl_->is_object(); }
bool Type::is_temporal() const { return impl_ && impl_->is_temporal(); }


template<typename T> bool Type::can_be_read_as() const { return false; }
template<> bool Type::can_be_read_as<int8_t>()   const { return impl_ && impl_->can_be_read_as_int8(); }
template<> bool Type::can_be_read_as<int16_t>()  const { return impl_ && impl_->can_be_read_as_int16(); }
template<> bool Type::can_be_read_as<int32_t>()  const { return impl_ && impl_->can_be_read_as_int32(); }
template<> bool Type::can_be_read_as<int64_t>()  const { return impl_ && impl_->can_be_read_as_int64(); }
template<> bool Type::can_be_read_as<float>()    const { return impl_ && impl_->can_be_read_as_float32(); }
template<> bool Type::can_be_read_as<double>()   const { return impl_ && impl_->can_be_read_as_float64(); }
template<> bool Type::can_be_read_as<CString>()  const { return impl_ && impl_->can_be_read_as_cstring(); }
template<> bool Type::can_be_read_as<py::oobj>() const { return impl_ && impl_->can_be_read_as_pyobject(); }


bool Type::operator==(const Type& other) const {
  return (impl_ == other.impl_) || (impl_ && impl_->equals(other.impl_));
}

Type::operator bool() const {
  return (impl_ != nullptr);
}

std::string Type::to_string() const {
  if (!impl_) return "Type()";
  return impl_->to_string();
}

Column Type::cast_column(Column&& col) const {
  xassert(impl_);
  return impl_->cast_column(std::move(col));
}



}  // namespace dt
