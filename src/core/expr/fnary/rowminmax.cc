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
#include <utility>
namespace dt {
namespace expr {


template <bool MIN, bool RETARGS>
std::string FExpr_RowMinMax<MIN,RETARGS>::name() const {
  return MIN? "rowmin" : "rowmax";
}

template <typename T1, typename T2, bool MIN, bool RETARGS=false>
static bool op_rowminmax(size_t i, T2* out, const colvec& columns) {
  bool minmax_valid = false;
  std::pair<T1,int64_t> minmax(0, 0);
  int64_t count = -1;
  for (const auto& col : columns) {
    T1 x;
    bool xvalid = col.get_element(i, &x);
    ++count;
    if (!xvalid) continue;
    if (minmax_valid) {
      if (MIN) {
        if (x < minmax.first) {
          minmax.first = x;
          minmax.second = count;
        }
      } else {
        if (x > minmax.first) {
          minmax.first = x;
          minmax.second = count;
        }
      }
    } else {
      minmax.first = x;
      minmax.second = count;
      minmax_valid = true;
    }
  }
  if (RETARGS) *out = minmax.second;
  else *out = minmax.first;
  return minmax_valid;
}

template <typename T, bool MIN, bool RETARGS>
static inline Column _rowminmax(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  if (RETARGS) {
    auto fn = op_rowminmax<T, int64_t, MIN, RETARGS>;
    return Column(new FuncNary_ColumnImpl<int64_t>(
          std::move(columns), fn, nrows, stype_from<int64_t>));
  } else {
    auto fn = op_rowminmax<T, T, MIN>;
    return Column(new FuncNary_ColumnImpl<T>(
          std::move(columns), fn, nrows, stype_from<T>));
  }
}


template <bool MIN, bool RETARGS>
Column FExpr_RowMinMax<MIN,RETARGS>::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  SType res_stype = common_numeric_stype(columns);
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowminmax<int32_t, MIN, RETARGS>(std::move(columns));
    case SType::INT64:   return _rowminmax<int64_t, MIN, RETARGS>(std::move(columns));
    case SType::FLOAT32: return _rowminmax<float, MIN, RETARGS>(std::move(columns));
    case SType::FLOAT64: return _rowminmax<double, MIN, RETARGS>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowminmax()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}

template class FExpr_RowMinMax<true,true>;
template class FExpr_RowMinMax<false,true>;
template class FExpr_RowMinMax<true,false>;
template class FExpr_RowMinMax<false,false>;


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

DECLARE_PYFN(&py_rowfn)
    ->name("rowargmax")
    ->docs(doc_dt_rowargmax)
    ->allow_varargs()
    ->add_info(FN_ROWARGMAX);

DECLARE_PYFN(&py_rowfn)
    ->name("rowargmin")
    ->docs(doc_dt_rowargmin)
    ->allow_varargs()
    ->add_info(FN_ROWARGMIN);


}}  // namespace dt::expr
