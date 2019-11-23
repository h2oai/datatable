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
#include <cmath>
#include "expr/fnary/fnary.h"
#include "column/const.h"
#include "column/func_nary.h"
namespace dt {
namespace expr {



static const char* doc_rowsd =
R"(rowsd(x1, x2, ...)
--

For each row, find the standard deviation of values in columns x1,
x2, ... The columns must be all numeric (boolean, integer or float).
The result will be a single float column with the same number of rows
as the input columns.

If any column contains an NA value, it will be skipped during the
calculation. Thus, NAs are treated as if they were zeros. If a row
contains only NA values, this function will produce an NA too.
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
    mean += tmp1 / count;
    T tmp2 = value - mean;
    m2 += tmp1 * tmp2;
  }
  if (count > 1 && !std::isnan(m2)) {
    *out = (m2 >= 0)? std::sqrt(m2 / (count - 1)) : 0;
    return true;
  } else {
    return false;
  }
}


template <typename T>
static inline Column _rowsd(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), op_rowsd<T>, nrows, stype_from<T>()));
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
    default: xassert(0);
  }
}




}}  // namespace dt::expr
