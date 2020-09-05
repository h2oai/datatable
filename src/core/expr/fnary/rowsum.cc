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
#include "column/const.h"
#include "column/func_nary.h"
#include "expr/fnary/fnary.h"
#include "models/utils.h"
namespace dt {
namespace expr {



static const char* doc_rowsum =
R"(rowsum(cols)
--

For each row, calculate the sum of all values in `cols`. Missing values
are treated as if they are zeros and skipped during the calcultion.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column and the same number
    of rows as in `cols`. The stype of the resulting column
    will be the smallest common stype calculated for `cols`,
    but not less than `int32`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.

See Also
--------

- :func:`rowcount()` -- count non-missing values row-wise.

)";

py::PKArgs args_rowsum(0, 0, 0, true, false, {}, "rowsum", doc_rowsum);



template <typename T>
static bool op_rowsum(size_t i, T* out, const colvec& columns) {
  T sum = 0;
  for (const auto& col : columns) {
    T x;
    bool xvalid = col.get_element(i, &x);
    if (xvalid) {
      sum += x;
    }
  }
  *out = sum;
  return _notnan(sum);
}


template <typename T>
static inline Column _rowsum(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), op_rowsum<T>, nrows, stype_from<T>));
}


Column naryop_rowsum(colvec&& columns) {
  if (columns.empty()) {
    return Const_ColumnImpl::make_int_column(1, 0, SType::INT32);
  }
  SType res_stype = detect_common_numeric_stype(columns, "rowsum");
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowsum<int32_t>(std::move(columns));
    case SType::INT64:   return _rowsum<int64_t>(std::move(columns));
    case SType::FLOAT32: return _rowsum<float>(std::move(columns));
    case SType::FLOAT64: return _rowsum<double>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowsum()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}




}}  // namespace dt::expr
