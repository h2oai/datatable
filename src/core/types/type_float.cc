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
#include <limits>
#include "column/cast.h"
#include "python/float.h"
#include "types/type_float.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// Type_Float32
//------------------------------------------------------------------------------

Type_Float32::Type_Float32()
  : Type_Numeric(SType::FLOAT32) {}


bool Type_Float32::is_float() const {
  return true;
}

bool Type_Float32::can_be_read_as_float32() const {
  return true;
}

std::string Type_Float32::to_string() const {
  return "float32";
}

py::oobj Type_Float32::min() const {
  return py::ofloat(-std::numeric_limits<float>::max());
}

py::oobj Type_Float32::max() const {
  return py::ofloat(std::numeric_limits<float>::max());
}

const char* Type_Float32::struct_format() const {
  return "f";
}

// Cast column `col` into type `float32`. The following conversions are
// supported:
//   - VOID->FLOAT32:    <all values are NAs>
//   - BOOL->FLOAT32:    True->1.0, False->0.0
//   - INT->FLOAT32:     `static_cast<float>(x)`
//   - FLOAT64->FLOAT32: `static_cast<float>(x)`
//   - DATE32->FLOAT32:  DATE32->INT32->FLOAT32
//   - TIME64->FLOAT32:  TIME64->INT64->FLOAT32
//   - STR->FLOAT32:     <parse float from string>
//   - OBJ->FLOAT32:     `x.to_pyfloat_force()`
//
Column Type_Float32::cast_column(Column&& col) const {
  constexpr SType st = SType::FLOAT32;
  switch (col.stype()) {
    case SType::VOID:    return Column::new_na_column(col.nrows(), st);
    case SType::BOOL:    return Column(new CastBool_ColumnImpl(st, std::move(col)));
    case SType::INT8:    return Column(new CastNumeric_ColumnImpl<int8_t>(st, std::move(col)));
    case SType::INT16:   return Column(new CastNumeric_ColumnImpl<int16_t>(st, std::move(col)));
    case SType::DATE32:
    case SType::INT32:   return Column(new CastNumeric_ColumnImpl<int32_t>(st, std::move(col)));
    case SType::TIME64:
    case SType::INT64:   return Column(new CastNumeric_ColumnImpl<int64_t>(st, std::move(col)));
    case SType::FLOAT32: return std::move(col);
    case SType::FLOAT64: return Column(new CastNumeric_ColumnImpl<double>(st, std::move(col)));
    case SType::STR32:
    case SType::STR64:   return Column(new CastString_ColumnImpl(st, std::move(col)));
    case SType::OBJ:     return Column(new CastObject_ColumnImpl(st, std::move(col)));
    default:
      throw NotImplError() << "Unable to cast column of type `" << col.type()
                           << "` into `float32`";
  }
}




//------------------------------------------------------------------------------
// Type_Float64
//------------------------------------------------------------------------------

Type_Float64::Type_Float64()
  : Type_Numeric(SType::FLOAT64) {}


bool Type_Float64::is_float() const {
  return true;
}

bool Type_Float64::can_be_read_as_float64() const {
  return true;
}

std::string Type_Float64::to_string() const {
  return "float64";
}

py::oobj Type_Float64::min() const {
  return py::ofloat(-std::numeric_limits<double>::max());
}

py::oobj Type_Float64::max() const {
  return py::ofloat(std::numeric_limits<double>::max());
}

const char* Type_Float64::struct_format() const {
  return "d";
}

Column Type_Float64::cast_column(Column&& col) const {
  constexpr SType st = SType::FLOAT64;
  switch (col.stype()) {
    case SType::VOID:    return Column::new_na_column(col.nrows(), st);
    case SType::BOOL:    return Column(new CastBool_ColumnImpl(st, std::move(col)));
    case SType::INT8:    return Column(new CastNumeric_ColumnImpl<int8_t>(st, std::move(col)));
    case SType::INT16:   return Column(new CastNumeric_ColumnImpl<int16_t>(st, std::move(col)));
    case SType::INT32:   return Column(new CastNumeric_ColumnImpl<int32_t>(st, std::move(col)));
    case SType::INT64:   return Column(new CastNumeric_ColumnImpl<int64_t>(st, std::move(col)));
    case SType::FLOAT32: return Column(new CastNumeric_ColumnImpl<float>(st, std::move(col)));
    case SType::FLOAT64: return std::move(col);
    case SType::DATE32:  return Column(new CastDate32_ColumnImpl(st, std::move(col)));
    case SType::STR32:
    case SType::STR64:   return Column(new CastString_ColumnImpl(st, std::move(col)));
    case SType::OBJ:     return Column(new CastObject_ColumnImpl(st, std::move(col)));
    default:
      throw NotImplError() << "Unable to cast column of type `" << col.type()
                           << "` into `float64`";
  }
}




}  // namespace dt
