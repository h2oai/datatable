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



static const char* doc_rowall =
R"(rowall(x1, x2, ...)
--

For each row in a frame formed by concatenating columns x1, x2, ...
return True if all values in that row are True, or otherwise return
False.

This is a row-wise function: it applies to a sequence of 0 or more
boolean columns and produces a single boolean column of the same
shape.
)";

py::PKArgs args_rowall(0, 0, 0, true, false, {}, "rowall", doc_rowall);



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



Column naryop_rowall(colvec&& columns) {
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




}}  // namespace dt::expr
