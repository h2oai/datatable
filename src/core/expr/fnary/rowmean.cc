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
#include <cmath>
#include "column/const.h"
#include "column/func_nary.h"
#include "expr/fnary/fnary.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


std::string FExpr_RowMean::name() const {
  return "rowmean";
}


template <typename T>
static bool op_rowmean(size_t i, T* out, const colvec& columns) {
  T sum = 0;
  int count = 0;
  for (const auto& col : columns) {
    T x;
    bool xvalid = col.get_element(i, &x);
    if (!xvalid) continue;
    sum += x;
    count++;
  }
  if (count && !std::isnan(sum)) {
    *out = sum/static_cast<T>(count);
    return true;
  } else {
    return false;
  }
}


template <typename T>
static inline Column _rowmean(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), op_rowmean<T>, nrows, stype_from<T>));
}


Column FExpr_RowMean::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  SType res_stype = common_numeric_stype(columns);
  if (res_stype == SType::INT32 || res_stype == SType::INT64) {
    res_stype = SType::FLOAT64;
  }
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::FLOAT32: return _rowmean<float>(std::move(columns));
    case SType::FLOAT64: return _rowmean<double>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowmean()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}



static const char* doc_rowmean =
R"(rowmean(*cols)
--

For each row, find the mean values among the columns from `cols`
skipping missing values. If a row contains only the missing values,
this function will produce a missing value too.


Parameters
----------
cols: FExpr
    Input columns.

return: FExpr
    f-expression consisting of one column that has the same number of rows
    as in `cols`. The column stype is `float32` when all the `cols`
    are `float32`, and `float64` in all the other cases.

except: TypeError
    The exception is raised when `cols` has non-numeric columns.

Examples
--------
::

    >>> from datatable import dt, f, rowmean
    >>> DT = dt.Frame({'a': [None, True, True, True],
    ...                'b': [2, 2, 1, 0],
    ...                'c': [3, 3, 1, 0],
    ...                'd': [0, 4, 6, 0],
    ...                'q': [5, 5, 1, 0]}

    >>> DT
       |     a      b      c      d      q
       | bool8  int32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 |    NA      2      3      0      5
     1 |     1      2      3      4      5
     2 |     1      1      1      6      1
     3 |     1      0      0      0      0
    [4 rows x 5 columns]


Get the row mean of all columns::

    >>> DT[:, rowmean(f[:])]
       |      C0
       | float64
    -- + -------
     0 |     2.5
     1 |     3
     2 |     2
     3 |     0.2
    [4 rows x 1 column]

Get the row mean of specific columns::

    >>> DT[:, rowmean(f['a', 'b', 'd'])]
       |       C0
       |  float64
    -- + --------
     0 | 1
     1 | 2.33333
     2 | 2.66667
     3 | 0.333333
    [4 rows x 1 column]



See Also
--------
- :func:`rowsd()` -- calculate the standard deviation row-wise.
)";

DECLARE_PYFN(&py_rowfn)
    ->name("rowmean")
    ->docs(doc_rowmean)
    ->allow_varargs()
    ->add_info(FN_ROWMEAN);




}}  // namespace dt::expr
