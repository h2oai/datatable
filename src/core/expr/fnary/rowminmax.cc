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


template <bool MIN, bool ARG>
FExpr_RowMinMax<MIN, ARG>::FExpr_RowMinMax(ptrExpr&& args)
  : FExpr_RowFn(std::move(args), ARG)
{}


template <bool MIN, bool ARG>
std::string FExpr_RowMinMax<MIN, ARG>::name() const {
  if (ARG) {
    return MIN? "rowargmin" : "rowargmax";
  } else {
    return MIN? "rowmin" : "rowmax";
  }
}



template <typename T, typename U, bool MIN, bool ARG>
static bool op_rowargminmax(size_t i, U* out, const colvec& columns) {
  bool minmax_valid = false;
  std::pair<T, size_t> minmax(0, 0);

  for (size_t j = 0; j < columns.size(); ++j) {
    T x;
    bool xvalid = columns[j].get_element(i, &x);
    if (!xvalid) continue;
    if (minmax_valid) {
      if (MIN) { if (x < minmax.first) minmax = std::make_pair(x, j); }
      else     { if (x > minmax.first) minmax = std::make_pair(x, j); }
    } else {
      minmax = std::make_pair(x, j);
      minmax_valid = true;
    }
  }

  *out = ARG? static_cast<U>(minmax.second)
            : static_cast<U>(minmax.first);

  return minmax_valid;
}



template <typename T, bool MIN, bool ARG>
static inline Column _rowminmax(colvec&& columns) {
  size_t nrows = columns[0].nrows();

  if (ARG) {
    auto fn = op_rowargminmax<T, int64_t, MIN, ARG>;
    return Column(new FuncNary_ColumnImpl<int64_t>(
             std::move(columns), fn, nrows, stype_from<int64_t>
           ));

  } else {
    auto fn = op_rowargminmax<T, T, MIN, ARG>;
    return Column(new FuncNary_ColumnImpl<T>(
            std::move(columns), fn, nrows, stype_from<T>
           ));
  }
}


template <bool MIN, bool ARG>
Column FExpr_RowMinMax<MIN,ARG>::apply_function(colvec&& columns,
                                                const size_t nrows,
                                                const size_t) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(nrows);
  }
  SType res_stype = common_numeric_stype(columns);
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowminmax<int32_t, MIN, ARG>(std::move(columns));
    case SType::INT64:   return _rowminmax<int64_t, MIN, ARG>(std::move(columns));
    case SType::FLOAT32: return _rowminmax<float, MIN, ARG>(std::move(columns));
    case SType::FLOAT64: return _rowminmax<double, MIN, ARG>(std::move(columns));
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
