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
#include "column/cast.h"
#include "column/time_scaled.h"
#include "frame/py_frame.h"
#include "stype.h"
#include "types/type_time.h"
#include "types/type_invalid.h"
namespace dt {



Type_Time64::Type_Time64() 
  : TypeImpl(SType::TIME64) {}


bool Type_Time64::can_be_read_as_int64() const { 
  return true; 
}

bool Type_Time64::is_temporal() const {
  return true; 
}

std::string Type_Time64::to_string() const { 
  return "time64"; 
}

// The min/max values are rounded to microsecond resolution, because
// that's the resolution that python supports.
py::oobj Type_Time64::min() const {
  return py::odatetime(-9223285636854775000L);
}

py::oobj Type_Time64::max() const {
  return py::odatetime(9223372036854775000L);
}

const char* Type_Time64::struct_format() const { 
  return "q";  // Pretend this is int64
}

TypeImpl* Type_Time64::common_type(TypeImpl* other) {
  if (other->is_temporal() || other->is_void()) {
    return this;
  }
  if (other->is_object() || other->is_invalid()) {
    return other;
  }
  return new Type_Invalid();
}


// Convert colum `col` into `time64` type. The following conversions are
// supported:
//   VOID->TIME64:   (all-NA columnn)
//   INT32->TIME64:  INT32->INT64<=>TIME64
//   INT64->TIME64:  INT64<=>TIME64
//   FLOAT->TIME64:  FLOAT->INT64<=>TIME64
//   DATE32->TIME64: (convert days into timestamps)
//   STR->TIME64:    (parse string as time64)
//   OBJ->TIME64:    (parse object as time64)
//
Column Type_Time64::cast_column(Column&& col) const {
  constexpr SType st = SType::TIME64;
  constexpr int64_t SECONDS = 1000000000;
  constexpr int64_t DAYS = 24 * 3600 * SECONDS;
  switch (col.stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), st);

    case SType::INT32:
      return Column(new CastNumeric_ColumnImpl<int32_t>(st, std::move(col)));

    case SType::INT64:
      col.replace_type_unsafe(Type::time64());
      return std::move(col);

    case SType::FLOAT32:
      return Column(new CastNumeric_ColumnImpl<float>(st, std::move(col)));

    case SType::FLOAT64:
      return Column(new CastNumeric_ColumnImpl<double>(st, std::move(col)));

    case SType::DATE32: {
      auto i64col = Column(
          new CastNumeric_ColumnImpl<int32_t>(SType::INT64, std::move(col))
      );
      return Column(new TimeScaled_ColumnImpl(std::move(i64col), DAYS));
    }

    case SType::TIME64:
      return std::move(col);

    case SType::OBJ:
      return Column(new CastObjToTime64_ColumnImpl(std::move(col)));

    case SType::STR32:
    case SType::STR64:
      return Column(new CastStringToTime64_ColumnImpl(std::move(col)));

    default:
      throw TypeError() << "Unable to cast column of type `" << col.type()
                        << "` into `time64`";
  }
}




}  // namespace dt
