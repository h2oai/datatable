//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
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
namespace dt {
namespace expr {


std::string FExpr_RowAll::name() const {
  return "rowall";
}



static bool op_rowall(size_t i, int8_t* out, const colvec& columns) {
  for (const auto& col : columns) {
    int8_t x;
    bool xvalid = col.get_element(i, &x);
    if (!xvalid || x == 0) {
      *out = 0;
      return true;
    }
  }
  *out = 1;
  return true;
}


Column FExpr_RowAll::apply_function(colvec&& columns,
                                    const size_t nrows,
                                    const size_t ncols) const
{
  // No columns
  if (ncols == 0) {
    return Const_ColumnImpl::make_bool_column(1, true);
  }
  // Some void columns
  if (columns.size() != ncols) {
    return Const_ColumnImpl::make_bool_column(nrows, false);
  }
  // No void columns
  for (size_t i = 0; i < columns.size(); ++i) {
    xassert(columns[i].nrows() == nrows);
    if (!columns[i].type().is_boolean()) {
      throw TypeError() << "Function `rowall` requires a sequence of boolean "
                           "columns, however column " << i << " has type `"
                        << columns[i].stype() << "`";
    }
  }
  return Column(new FuncNary_ColumnImpl<int8_t>(
                      std::move(columns), op_rowall, nrows, SType::BOOL));
}


DECLARE_PYFN(&py_rowfn)
    ->name("rowall")
    ->docs(dt::doc_dt_rowall)
    ->allow_varargs()
    ->add_info(FN_ROWALL);




}}  // namespace dt::expr
