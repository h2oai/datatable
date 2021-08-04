//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include <algorithm>
#include "column/const.h"
#include "column/func_nary.h"
#include "documentation.h"
#include "expr/fnary/fnary.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


template <bool MIN>
std::string FExpr_RowMinMax<MIN>::name() const {
  return MIN? "rowmin" : "rowmax";
}



template <typename T, bool MIN>
static bool op_rowminmax(size_t i, T* out, const colvec& columns) {
  bool minmax_valid = false;
  T minmax = 0;
  for (const auto& col : columns) {
    T x;
    bool xvalid = col.get_element(i, &x);
    if (!xvalid) continue;
    if (minmax_valid) {
      if (MIN) { if (x < minmax) minmax = x; }
      else     { if (x > minmax) minmax = x; }
    } else {
      minmax = x;
      minmax_valid = true;
    }
  }
  *out = minmax;
  return minmax_valid;
}


template <typename T, bool MIN>
static inline Column _rowminmax(colvec&& columns) {
  auto fn = op_rowminmax<T, MIN>;
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), fn, nrows, stype_from<T>));
}


template <bool MIN>
Column FExpr_RowMinMax<MIN>::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  SType res_stype = common_numeric_stype(columns);
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowminmax<int32_t, MIN>(std::move(columns));
    case SType::INT64:   return _rowminmax<int64_t, MIN>(std::move(columns));
    case SType::FLOAT32: return _rowminmax<float, MIN>(std::move(columns));
    case SType::FLOAT64: return _rowminmax<double, MIN>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowminmax()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}

template class FExpr_RowMinMax<true>;
template class FExpr_RowMinMax<false>;


DECLARE_PYFN(&py_rowfn)
    ->name("rowmin")
    ->docs(doc_dt_rowmin)
    ->allow_varargs()
    ->add_info(FN_ROWMIN);

DECLARE_PYFN(&py_rowfn)
    ->name("rowmax")
    ->docs(doc_dt_rowmax)
    ->allow_varargs()
    ->add_info(FN_ROWMAX);




}}  // namespace dt::expr
