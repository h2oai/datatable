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
#include "column/cast.h"
#include "types/type_object.h"
#include "stype.h"
namespace dt {



Type_Object::Type_Object() 
  : TypeImpl(SType::OBJ) {}


bool Type_Object::is_object() const {
  return true;
}

bool Type_Object::can_be_read_as_pyobject() const {
  return true;
}

std::string Type_Object::to_string() const {
  return "obj64";
}

TypeImpl* Type_Object::common_type(TypeImpl* other) {
  if (other->is_invalid()) return other;
  return this;
}

const char* Type_Object::struct_format() const {
  return "O";
}


// Cast column `col` into the `obj64` type. Ideally, this should support
// all target types.
//
Column Type_Object::cast_column(Column&& col) const {
  constexpr auto st = SType::OBJ;
  switch (col.stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), st);

    case SType::BOOL:
      return Column(new CastBool_ColumnImpl(st, std::move(col)));

    case SType::INT8:
      return Column(new CastNumeric_ColumnImpl<int8_t>(st, std::move(col)));

    case SType::INT16:
      return Column(new CastNumeric_ColumnImpl<int16_t>(st, std::move(col)));

    case SType::INT32:
      return Column(new CastNumeric_ColumnImpl<int32_t>(st, std::move(col)));

    case SType::INT64:
      return Column(new CastNumeric_ColumnImpl<int64_t>(st, std::move(col)));

    case SType::FLOAT32:
      return Column(new CastNumeric_ColumnImpl<float>(st, std::move(col)));

    case SType::FLOAT64:
      return Column(new CastNumeric_ColumnImpl<double>(st, std::move(col)));

    case SType::DATE32:
      return Column(new CastDate32_ColumnImpl(st, std::move(col)));

    case SType::TIME64:
      return Column(new CastTime64ToObj64_ColumnImpl(std::move(col)));

    case SType::STR32:
    case SType::STR64:
      return Column(new CastString_ColumnImpl(st, std::move(col)));

    case SType::OBJ:
      return std::move(col);

    case SType::ARR32:
    case SType::ARR64:
      return Column(new CastArrayToObject_ColumnImpl(std::move(col)));

    default:
      throw NotImplError() << "Unable to cast column of type `" << col.type()
                        << "` into `obj64`";
  }
}




}  // namespace dt
