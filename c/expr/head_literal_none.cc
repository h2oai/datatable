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
#include "expr/head_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


Kind Head_Literal_None::get_expr_kind() const {
  return Kind::None;
}


Workframe Head_Literal_None::evaluate_n(const vecExpr&, EvalContext& ctx) const {
  return _wrap_column(ctx, Const_ColumnImpl::make_na_column(1));
}



// When used as j, `None` means select all columns
Workframe Head_Literal_None::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool) const
{
  size_t n = ctx.get_datatable(0)->ncols();
  Workframe outputs(ctx);
  for (size_t i = 0; i < n; ++i) {
    outputs.add_ref_column(0, i);
  }
  return outputs;
}


// When used in f, `None` means select nothing
Workframe Head_Literal_None::evaluate_f(EvalContext& ctx, size_t, bool) const {
  return Workframe(ctx);
}




}}  // namespace dt::expr
