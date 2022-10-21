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
#include "column/cast.h"
#include "stype.h"
#include "types/type_bool.h"
namespace dt {



Type_Bool8::Type_Bool8()
  : TypeImpl_Numeric(SType::BOOL) {}


bool Type_Bool8::is_boolean() const {
  return true;
}

bool Type_Bool8::can_be_read_as_int8() const {
  return true;
}


std::string Type_Bool8::to_string() const {
  return "bool8";
}

py::oobj Type_Bool8::min() const {
  return py::False();
}

py::oobj Type_Bool8::max() const {
  return py::True();
}

const char* Type_Bool8::struct_format() const {
  return "?";
}


// Cast column `col` into the boolean type.
// The following casts are supported:
//   - VOID->BOOL:   all values are NAs
//   - INT->BOOL:    0->False, !0->True
//   - FLOAT->BOOL:  0->False, !0->True
//   - STR->BOOL:    "False"->False, "True"->True
//   - OBJ->BOOL:    <Fale>->False, <True>->True
//   - DATE32->BOOL: -- (invalid)
//   - TIME64->BOOL: --
//
Column Type_Bool8::cast_column(Column&& col) const {
  switch (col.data_stype()) {
    case SType::VOID:
      return Column::new_na_column(col.nrows(), SType::BOOL);

    case SType::BOOL:
      if (col.type().is_categorical()) {
        col.replace_type_unsafe(Type::bool8());
      }
      return std::move(col);

    case SType::INT8:
      return Column(new CastNumericToBool_ColumnImpl<int8_t>(std::move(col)));

    case SType::INT16:
      return Column(new CastNumericToBool_ColumnImpl<int16_t>(std::move(col)));

    case SType::INT32:
      return Column(new CastNumericToBool_ColumnImpl<int32_t>(std::move(col)));

    case SType::INT64:
      return Column(new CastNumericToBool_ColumnImpl<int64_t>(std::move(col)));

    case SType::FLOAT32:
      return Column(new CastNumericToBool_ColumnImpl<float>(std::move(col)));

    case SType::FLOAT64:
      return Column(new CastNumericToBool_ColumnImpl<double>(std::move(col)));

    case SType::STR32:
    case SType::STR64:
      return Column(new CastStringToBool_ColumnImpl(std::move(col)));

    case SType::OBJ:
      return Column(new CastObjToBool_ColumnImpl(std::move(col)));

    default:
      throw TypeError() << "Unable to cast column of type `" << col.type()
                        << "` into `bool8`";
  }
}




}  // namespace dt
