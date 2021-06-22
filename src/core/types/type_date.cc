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
#include "frame/py_frame.h"
#include "stype.h"
#include "types/type_date.h"
#include "types/type_invalid.h"
namespace dt {



Type_Date32::Type_Date32() 
  : TypeImpl(SType::DATE32) {}


bool Type_Date32::can_be_read_as_int32() const { 
  return true; 
}

bool Type_Date32::is_time() const { 
  return true; 
}

std::string Type_Date32::to_string() const { 
  return "date32"; 
}

py::oobj Type_Date32::min() const {
  return py::odate(-std::numeric_limits<int>::max());
}

py::oobj Type_Date32::max() const {
  return py::odate(std::numeric_limits<int>::max() - 719468);
}

const char* Type_Date32::struct_format() const { 
  return "i";  // Pretend this is int32
}


TypeImpl* Type_Date32::common_type(TypeImpl* other) {
  if (other->stype() == SType::DATE32 || other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


// Convert colum `col` into `date32` type. The following conversions are
// supported:
//   VOID->DATE32:   (all-NA columnn)
//   INT32->DATE32:  (replace type on the column)
//   INT64->DATE32:  INT64->INT32<=>DATE32
//   FLOAT->DATE32:  --"--
//   TIME64->DATE32: (truncate time-of-day part)
//   STR->DATE32:    (parse string as date)
//   OBJ->DATE32:    (parse object as date)
//
Column Type_Date32::cast_column(Column&& col) const {
  constexpr SType st = SType::DATE32;
  switch (col.stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), st);

    case SType::INT32:
      col.replace_type_unsafe(Type::date32());
      return std::move(col);

    case SType::INT64:
      return Column(new CastNumeric_ColumnImpl<int64_t>(st, std::move(col)));

    case SType::FLOAT32:
      return Column(new CastNumeric_ColumnImpl<float>(st, std::move(col)));

    case SType::FLOAT64:
      return Column(new CastNumeric_ColumnImpl<double>(st, std::move(col)));

    case SType::DATE32:
      return std::move(col);

    case SType::TIME64:
      return Column(new CastTime64ToDate32_ColumnImpl(std::move(col)));

    case SType::STR32:
    case SType::STR64:
      return Column(new CastStringToDate32_ColumnImpl(std::move(col)));

    case SType::OBJ:
      return Column(new CastObjToDate32_ColumnImpl(std::move(col)));

    default:
      throw TypeError() << "Unable to cast column of type `" << col.type()
                        << "` into `date32`";
  }
}




}  // namespace dt
