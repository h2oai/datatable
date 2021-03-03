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


std::string FExpr_RowSd::name() const {
  return "rowsd";
}


template <typename T>
static bool op_rowsd(size_t i, T* out, const colvec& columns) {
  T mean = 0;
  T m2 = 0;
  int count = 0;
  for (const auto& col : columns) {
    T value;
    bool xvalid = col.get_element(i, &value);
    if (!xvalid) continue;
    count++;
    T tmp1 = value - mean;
    mean += tmp1 / static_cast<T>(count);
    T tmp2 = value - mean;
    m2 += tmp1 * tmp2;
  }
  if (count > 1 && !std::isnan(m2)) {
    *out = (m2 >= 0)? std::sqrt(m2 / static_cast<T>(count - 1)) : 0;
    return true;
  } else {
    return false;
  }
}


template <typename T>
static inline Column _rowsd(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), op_rowsd<T>, nrows, stype_from<T>));
}


Column FExpr_RowSd::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  SType res_stype = common_numeric_stype(columns);
  if (res_stype == SType::INT32 || res_stype == SType::INT64) {
    res_stype = SType::FLOAT64;
  }
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::FLOAT32: return _rowsd<float>(std::move(columns));
    case SType::FLOAT64: return _rowsd<double>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowsd()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}




static const char* doc_rowsd =
R"(rowsd(*cols)
--

For each row, find the standard deviation among the columns from `cols`
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

    >>> from datatable import dt, f, rowsd
    >>> DT = dt.Frame({'name': ['A', 'B', 'C', 'D', 'E'],
    ...                'group': ['mn', 'mn', 'kl', 'kl', 'fh'],
    ...                'S1': [1, 4, 5, 6, 7],
    ...                'S2': [2, 3, 8, 5, 1],
    ...                'S3': [8, 5, 2, 5, 3]}

    >>> DT
       | name   group     S1     S2     S3
       | str32  str32  int32  int32  int32
    -- + -----  -----  -----  -----  -----
     0 | A      mn         1      2      8
     1 | B      mn         4      3      5
     2 | C      kl         5      8      2
     3 | D      kl         6      5      5
     4 | E      fh         7      1      3
    [5 rows x 5 columns]

Get the row standard deviation for all integer columns::

    >>> DT[:, rowsd(f[int])]
       |      C0
       | float64
    -- + -------
     0 | 3.78594
     1 | 1
     2 | 3
     3 | 0.57735
     4 | 3.05505
    [5 rows x 1 column]

Get the row standard deviation for some columns::

    >>> DT[:, rowsd(f[2, 3])]
       |       C0
       |  float64
    -- + --------
     0 | 0.707107
     1 | 0.707107
     2 | 2.12132
     3 | 0.707107
     4 | 4.24264
    [5 rows x 1 column]


See Also
--------
- :func:`rowmean()` -- calculate the mean value row-wise.
)";

DECLARE_PYFN(&py_rowfn)
    ->name("rowsd")
    ->docs(doc_rowsd)
    ->allow_varargs()
    ->add_info(FN_ROWSD);



}}  // namespace dt::expr
