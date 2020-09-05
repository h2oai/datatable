//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "expr/fnary/fnary.h"
#include "column/const.h"
#include "column/func_nary.h"
namespace dt {
namespace expr {



static const char* doc_rowsd =
R"(rowsd(cols)
--

For each row, find the standard deviation among the columns from `cols`
skipping missing values. If a row contains only the missing values,
this function will produce a missing value too.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column and the same number of rows
    as in `cols`. The column stype is `float32` when all the `cols`
    are `float32`, and `float64` in all the other cases.

except: TypeError
    The exception is raised when `cols` has non-numeric columns.

See Also
--------

- :func:`rowmean()` -- calculate the mean value row-wise.

)";

py::PKArgs args_rowsd(0, 0, 0, true, false, {}, "rowsd", doc_rowsd);



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


Column naryop_rowsd(colvec&& columns) {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  SType res_stype = detect_common_numeric_stype(columns, "rowsd");
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




}}  // namespace dt::expr
