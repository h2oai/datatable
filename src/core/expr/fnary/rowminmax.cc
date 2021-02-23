//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include "expr/fnary/fnary.h"
#include "column/const.h"
#include "column/func_nary.h"
namespace dt {
namespace expr {


static const char* doc_rowmin =
R"(rowmin(cols)
--

For each row, find the smallest value among the columns from `cols`,
excluding missing values.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column that has the same number of rows
    as in `cols`. The column stype is the smallest common stype
    for `cols`, but not less than `int32`.

except: TypeError
    The exception is raised when `cols` has non-numeric columns.

See Also
--------

- :func:`rowmax()` -- find the largest element row-wise.

Example
-------
::

    >>> from datatable import dt, f
    >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
    ...                "B": [None, 2, 3, 4, None],
    ...                "C":[True, False, False, True, True]})
    >>> DT
       |     A      B      C
       | int32  int32  bool8
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      0
     2 |     2      3      0
     3 |     1      4      1
     4 |     2     NA      1
    [5 rows x 3 columns]

::

    >>> DT[:, dt.rowmin(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     0
     2 |     0
     3 |     1
     4 |     1
    [5 rows x 1 column]


)";


static const char* doc_rowmax =
R"(rowmax(cols)
--

For each row, find the largest value among the columns from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column that has the same number of rows
    as in `cols`. The column stype is the smallest common stype
    for `cols`, but not less than `int32`.

except: TypeError
    The exception is raised when `cols` has non-numeric columns.

See Also
--------

- :func:`rowmin()` -- find the smallest element row-wise.

Example
-------
::

    >>> from datatable import dt, f
    >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
    ...                "B": [None, 2, 3, 4, None],
    ...                "C":[True, False, False, True, True]})
    >>> DT
       |     A      B      C
       | int32  int32  bool8
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      0
     2 |     2      3      0
     3 |     1      4      1
     4 |     2     NA      1
    [5 rows x 3 columns]

::

    >>> DT[:, dt.rowmax(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     2
     2 |     3
     3 |     4
     4 |     2
    [5 rows x 1 column]


)";

py::PKArgs args_rowmin(0, 0, 0, true, false, {}, "rowmin", doc_rowmin);
py::PKArgs args_rowmax(0, 0, 0, true, false, {}, "rowmax", doc_rowmax);



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


template <typename T>
static inline Column _rowminmax(colvec&& columns, bool MIN) {
  auto fn = MIN? op_rowminmax<T, true>
               : op_rowminmax<T, false>;
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), fn, nrows, stype_from<T>));
}


Column naryop_rowminmax(colvec&& columns, bool MIN) {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  const char* fnname = MIN? "rowmin" : "rowmax";
  SType res_stype = detect_common_numeric_stype(columns, fnname);
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowminmax<int32_t>(std::move(columns), MIN);
    case SType::INT64:   return _rowminmax<int64_t>(std::move(columns), MIN);
    case SType::FLOAT32: return _rowminmax<float>(std::move(columns), MIN);
    case SType::FLOAT64: return _rowminmax<double>(std::move(columns), MIN);
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowminmax()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}




}}  // namespace dt::expr
