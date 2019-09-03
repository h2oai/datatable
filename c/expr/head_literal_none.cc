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
#include "column/column_const.h"
#include "expr/head_literal.h"
#include "expr/outputs.h"
namespace dt {
namespace expr {



Column Head_Literal_None::eval_as_literal() const {
  return Const_ColumnImpl::make_na_column(1);
}


Outputs Head_Literal_None::eval_as_selector(workframe&, size_t) const {
  throw RuntimeError();  // LCOV_EXCL_LINE
}


// When used as j, `None` means select all columns
Outputs Head_Literal_None::evaluate_j(const vecExpr&, workframe& wf) const {
  auto dt0 = wf.get_datatable(0);
  Outputs res;
  for (size_t i = 0; i < dt0->ncols; ++i) {
    res.add_column(dt0, i);
  }
  return res;
}


// When used in f, `None` means select nothing
Outputs Head_Literal_None::evaluate_f(const vecExpr&, workframe&, size_t) const {
  return Outputs();
}




}}  // namespace dt::expr
