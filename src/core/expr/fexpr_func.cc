//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "expr/fexpr_func.h"
#include "expr/workframe.h"
#include "stype.h"
namespace dt {
namespace expr {



Kind FExpr_Func::get_expr_kind() const {
  return Kind::Func;
}

int FExpr_Func::precedence() const noexcept {
  return 16;
}


// Forbid expressions like `f[f.A]`.
Workframe FExpr_Func::evaluate_f(EvalContext&, size_t) const {
  throw TypeError() << "An expression cannot be used as an f-selector";
}


// When used as j node, a Func expression means exactly the same as
// evaluating this expression in "normal" mode.
Workframe FExpr_Func::evaluate_j(EvalContext& ctx) const {
  return evaluate_n(ctx);
}


// When used as a replacement target, a Func expression behaves the
// same as during evaluation in "normal" mode.
Workframe FExpr_Func::evaluate_r(EvalContext& ctx, const sztvec&) const {
  return evaluate_n(ctx);
}


// When used as i node, we evaluate the Func expression normally, and
// then convert the resulting boolean column into a RowIndex.
RowIndex FExpr_Func::evaluate_i(EvalContext& ctx) const {
  Workframe wf = evaluate_n(ctx);
  if (wf.ncols() != 1) {
    throw TypeError() << "i-expression evaluated into " << wf.ncols()
        << " columns";
  }
  Column col = wf.retrieve_column(0);
  if (col.stype() != SType::BOOL) {
    throw TypeError() << "Filter expression must be boolean, instead it "
        "was of type " << col.stype();
  }
  return RowIndex(std::move(col));
}


RiGb FExpr_Func::evaluate_iby(EvalContext&) const {
  throw NotImplError() << "FExpr_Func::evaluate_iby() not implemented yet";
}





}}  // namespace dt::expr
