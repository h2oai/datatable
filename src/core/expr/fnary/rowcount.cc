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
#include "expr/funary/umaker.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


std::string FExpr_RowCount::name() const {
  return "rowcount";
}



static bool op_rowcount(size_t i, int32_t* out, const colvec& columns) {
  int32_t valid_count = static_cast<int32_t>(columns.size());
  for (const auto& col : columns) {
    int8_t x;
    // each column is ISNA(col), so the return value is 1 if
    // the value in the original column is NA.
    col.get_element(i, &x);
    valid_count -= x;
  }
  *out = valid_count;
  return true;
}


Column FExpr_RowCount::apply_function(colvec&& columns,
                                      const size_t nrows,
                                      const size_t) const
{
  if (columns.empty()) {
    return Const_ColumnImpl::make_int_column(nrows, 0, SType::INT32);
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    xassert(columns[i].nrows() == nrows);
    columns[i] = unaryop(Op::ISNA, std::move(columns[i]));
  }
  return Column(new FuncNary_ColumnImpl<int32_t>(
                      std::move(columns), op_rowcount, nrows, SType::INT32));
}

DECLARE_PYFN(&py_rowfn)
    ->name("rowcount")
    ->docs(doc_dt_rowcount)
    ->allow_varargs()
    ->add_info(FN_ROWCOUNT);




}}  // namespace dt::expr
