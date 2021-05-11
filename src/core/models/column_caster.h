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
 *  Infinite values are casted into NA's. This function is only needed
 *  as a workaround for a stats calculation: min/max on this column will never be
 *  an inf.
 */
template <typename T_from, typename T_to>
Column make_inf2na_casted_column(const Column& col, dt::SType stype) {
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

colvec make_casted_columns(const DataTable*, const dt::SType);


#endif
