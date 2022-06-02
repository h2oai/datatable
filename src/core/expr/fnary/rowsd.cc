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
#include "documentation.h"
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


Column FExpr_RowSd::apply_function(colvec&& columns,
                                   const size_t nrows,
                                   const size_t) const
{
  if (columns.empty()) {
    return Column(new ConstNa_ColumnImpl(nrows, SType::FLOAT64));
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


DECLARE_PYFN(&py_rowfn)
    ->name("rowsd")
    ->docs(doc_dt_rowsd)
    ->allow_varargs()
    ->add_info(FN_ROWSD);



}}  // namespace dt::expr
