//------------------------------------------------------------------------------
// Copyright 2021-2022 H2O.ai
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
#include "stype.h"
#include "types/typeimpl_numeric.h"
#include "types/type_invalid.h"
namespace dt {



bool TypeImpl_Numeric::is_numeric() const {
  return true; 
}


TypeImpl* TypeImpl_Numeric::common_type(TypeImpl* other) {
  if (other->is_numeric() || other->is_void()) {
    auto stype1 = static_cast<int>(this->stype());
    auto stype2 = static_cast<int>(other->stype());
    return stype1 >= stype2? this : other;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


// Cast column `col` into `this` type. The following conversions are
// supported:
//   - VOID->this:    <all values are NAs>
//   - BOOL->this:    True->1.0, False->0.0
//   - INT->this:     `static_cast<>(x)`
//   - FLOAT64->this: `static_cast<>(x)`
//   - DATE32->this:  DATE32->INT32->this
//   - TIME64->this:  TIME64->INT64->this
//   - STR->this:     <parse from string>
//   - OBJ->this:     <convert py object into int/float>
//
// Note that `Type_Bool8` (which inherits from TypeImpl_Numeric)
// overrides this method.
//
Column TypeImpl_Numeric::cast_column(Column&& col) const {
  const SType st = stype();
  switch (col.data_stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), st);

    case SType::BOOL:
      return Column(new CastBool_ColumnImpl(st, std::move(col)));

    case SType::INT8:
      return Column(new CastNumeric_ColumnImpl<int8_t>(st, std::move(col)));

    case SType::INT16:
      return Column(new CastNumeric_ColumnImpl<int16_t>(st, std::move(col)));

    case SType::DATE32:
    case SType::INT32:
      return Column(new CastNumeric_ColumnImpl<int32_t>(st, std::move(col)));

    case SType::TIME64:
    case SType::INT64:
      return Column(new CastNumeric_ColumnImpl<int64_t>(st, std::move(col)));

    case SType::FLOAT32:
      return Column(new CastNumeric_ColumnImpl<float>(st, std::move(col)));

    case SType::FLOAT64:
      return Column(new CastNumeric_ColumnImpl<double>(st, std::move(col)));

    case SType::STR32:
    case SType::STR64:
      return Column(new CastString_ColumnImpl(st, std::move(col)));

    case SType::OBJ:
      return Column(new CastObject_ColumnImpl(st, std::move(col)));

    default:
      throw TypeError() << "Unable to cast column of type `" << col.type()
                        << "` into `" << to_string() << "`";
  }
}




}  // namespace dt
