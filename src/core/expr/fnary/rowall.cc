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


Column FExpr_RowAll::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_bool_column(1, true);
  }
  size_t nrows = columns[0].nrows();
  for (size_t i = 0; i < columns.size(); ++i) {
    xassert(columns[i].nrows() == nrows);
    if (columns[i].stype() != SType::BOOL) {
      throw TypeError() << "Function `rowall` requires a sequence of boolean "
                           "columns, however column " << i << " has type `"
                        << columns[i].stype() << "`";
    }
  }
  return Column(new FuncNary_ColumnImpl<int8_t>(
                      std::move(columns), op_rowall, nrows, SType::BOOL));
}



static const char* doc_rowall =
R"(rowall(cols)
--

For each row in `cols` return `True` if all values in that row are `True`,
or otherwise return `False`.

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


Example
-------
::

    >>> from datatable import dt, f
    >>> DT = dt.Frame({"A":[True, True],
    ...                "B":[True, False],
    ...                "C":[True, True]})
    >>> DT
       |     A      B      C
       | bool8  bool8  bool8
    -- + -----  -----  -----
     0 |     1      1      1
     1 |     1      0      1
    [2 rows x 3 columns]

::

    >>> DT[:, dt.rowall(f[:])]
       |    C0
       | bool8
    -- + -----
     0 |     1
     1 |     0
    [2 rows x 1 column]


See Also
--------
- :func:`rowany()` -- row-wise `any() <https://docs.python.org/3/library/functions.html#any>`_ function.
)";

DECLARE_PYFN(&py_rowfn)
    ->name("rowall")
    ->docs(doc_rowall)
    ->allow_varargs()
    ->add_info(FN_ROWALL);




}}  // namespace dt::expr
