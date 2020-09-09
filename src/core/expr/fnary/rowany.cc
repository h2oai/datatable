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



static const char* doc_rowany =
R"(rowany(cols)
--

For each row in `cols` return `True` if any of the values in that row
are `True`, or otherwise return `False`. The function uses shortcut
evaluation: if the `True` value is found in one of the columns,
then the subsequent columns are skipped.

Parameters
----------
cols: Expr
    Input boolean columns.

return: Expr
    f-expression consisting of one boolean column that has the same number
    of rows as in `cols`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-boolean type.

See Also
--------

- :func:`rowall()` -- row-wise `all() <https://docs.python.org/3/library/functions.html#all>`_ function.

)";

py::PKArgs args_rowany(0, 0, 0, true, false, {}, "rowany", doc_rowany);



static bool op_rowany(size_t i, int8_t* out, const colvec& columns) {
  for (const auto& col : columns) {
    int8_t x;
    bool xvalid = col.get_element(i, &x);
    if (xvalid && x) {
      *out = 1;
      return true;
    }
  }
  *out = 0;
  return true;
}



Column naryop_rowany(colvec&& columns) {
  if (columns.empty()) {
    return Const_ColumnImpl::make_bool_column(1, true);
  }
  size_t nrows = columns[0].nrows();
  for (size_t i = 0; i < columns.size(); ++i) {
    xassert(columns[i].nrows() == nrows);
    if (columns[i].stype() != SType::BOOL) {
      throw TypeError() << "Function `rowany` requires a sequence of boolean "
                           "columns, however column " << i << " has type `"
                        << columns[i].stype() << "`";
    }
  }

  return Column(new FuncNary_ColumnImpl<int8_t>(
                      std::move(columns), op_rowany, nrows, SType::BOOL));
}




}}  // namespace dt::expr
