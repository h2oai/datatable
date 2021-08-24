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
#include "stype.h"
#include "types/type_invalid.h"
#include "types/type_array.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// Type_Array
//------------------------------------------------------------------------------

Type_Array::Type_Array(Type t, SType stype)
  : TypeImpl(stype),
    childType_(std::move(t)) {}


bool Type_Array::is_compound() const {
  return true;
}

bool Type_Array::is_array() const {
  return true;
}

bool Type_Array::can_be_read_as_column() const {
  return true;
}


TypeImpl* Type_Array::common_type(TypeImpl* other) {
  if (equals(other)) {
    return this;
  }
  if (other->is_array()) {
    auto stype0 = stype();
    auto stype1 = other->stype();
    auto stypeR = stype0 > stype1 ? stype0 : stype1;
    auto child0 = child_type();
    auto child1 = other->child_type();
    auto childR = Type::common(child0, child1);
    if (stypeR == stype0 && childR == child0) {
      return this;
    }
    if (stypeR == stype1 && childR == child1) {
      return other;
    }
    if (!childR.is_invalid()) {
      if (stypeR == SType::ARR32) return new Type_Arr32(childR);
      if (stypeR == SType::ARR64) return new Type_Arr64(childR);
    }
    // otherwise fall-through and return an invalid type
  }
  if (other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


bool Type_Array::equals(const TypeImpl* other) const {
  return other->stype() == stype() &&
         childType_ == reinterpret_cast<const Type_Array*>(other)->childType_;
}


size_t Type_Array::hash() const noexcept {
  return static_cast<size_t>(stype()) + STYPES_COUNT * childType_.hash();
}


Type Type_Array::child_type() const {
  return childType_;
}


// Cast column `col` into an array type. This should support all
// target types.
//
Column Type_Array::cast_column(Column&& col) const {
  const auto st = stype();
  switch (col.stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), st);

    // case SType::STR32:
    // case SType::STR64:
    //   if (st == col.stype()) return std::move(col);
    //   return Column(new CastString_ColumnImpl(st, std::move(col)));

    case SType::ARR32:
    case SType::ARR64: {

      // break;
    }
    case SType::OBJ:
      // return Column(new CastObject_ColumnImpl(st, std::move(col)));

    default:
      throw NotImplError() << "Unable to cast column of type `" << col.type()
                           << "` into `" << to_string() << "`";
  }
}






//------------------------------------------------------------------------------
// Type_Arr32
//------------------------------------------------------------------------------

Type_Arr32::Type_Arr32(Type t)
  : Type_Array(std::move(t), SType::ARR32) {}


std::string Type_Arr32::to_string() const {
  return "arr32(" + childType_.to_string() + ")";
}




//------------------------------------------------------------------------------
// Type_Arr64
//------------------------------------------------------------------------------

Type_Arr64::Type_Arr64(Type t)
  : Type_Array(std::move(t), SType::ARR64) {}


std::string Type_Arr64::to_string() const {
  return "arr64(" + childType_.to_string() + ")";
}




}  // namespace dt
