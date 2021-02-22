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

#ifndef dt_MODELS_COLUMN_CASTER_h
#define dt_MODELS_COLUMN_CASTER_h
#include "column/func_unary.h"


/**
 *  Create a virtual column that casts numeric `col` from `T_from` to `T_to`.
 *  Infinite values are casted into NA's.
 */
template <typename T_from, typename T_to>
Column make_casted_column(const Column& col, dt::SType stype) {
  Column col_casted = Column(new dt::FuncUnary2_ColumnImpl<T_from, T_to>(
           Column(col),
           [](T_from x, bool x_isvalid, T_to* out) {
             *out = static_cast<T_to>(x);
             return x_isvalid && _isfinite(x);
           },
           col.nrows(),
           stype
         ));

  return col_casted;
}


/**
 *  Create a vector of numeric columns casted to `T`.
 */
template <typename T>
colvec make_casted_columns(const DataTable* dt, const dt::SType stype) {
  colvec cols_casted;
  const size_t ncols = dt->ncols();
  cols_casted.reserve(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    const Column& col = dt->get_column(i);
    Column col_casted;
    switch (stype) {
      case dt::SType::VOID:
      case dt::SType::BOOL:
      case dt::SType::INT8:    col_casted = make_casted_column<int8_t, T>(col, stype); break;
      case dt::SType::INT16:   col_casted = make_casted_column<int16_t, T>(col, stype); break;
      case dt::SType::INT32:   col_casted = make_casted_column<int32_t, T>(col, stype); break;
      case dt::SType::INT64:   col_casted = make_casted_column<int64_t, T>(col, stype); break;
      case dt::SType::FLOAT32: col_casted = make_casted_column<float, T>(col, stype); break;
      case dt::SType::FLOAT64: col_casted = make_casted_column<double, T>(col, stype); break;
      default: throw TypeError() << "Columns with stype `" << stype << "` are not supported";
    }
    cols_casted.push_back(std::move(col_casted));
  }

  return cols_casted;
}

#endif
