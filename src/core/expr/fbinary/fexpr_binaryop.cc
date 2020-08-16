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
#include "expr/fbinary/fexpr_binaryop.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {



FExpr_BinaryOp::FExpr_BinaryOp(ptrExpr&& x, ptrExpr&& y)
  : lhs_(std::move(x)),
    rhs_(std::move(y))
{}


Workframe FExpr_BinaryOp::evaluate_n(EvalContext& ctx) const {
  Workframe lhswf = lhs_->evaluate_n(ctx);
  Workframe rhswf = rhs_->evaluate_n(ctx);
  if (lhswf.ncols() == 1) lhswf.repeat_column(rhswf.ncols());
  if (rhswf.ncols() == 1) rhswf.repeat_column(lhswf.ncols());
  if (lhswf.ncols() != rhswf.ncols()) {
    throw ValueError() << "Incompatible column vectors in a binary operation `"
      << repr() << "`: LHS contains " << lhswf.ncols()
      << " columns, while RHS has " << rhswf.ncols() << " columns";
  }
  lhswf.sync_grouping_mode(rhswf);
  auto gmode = lhswf.get_grouping_mode();
  Workframe outputs(ctx);
  for (size_t i = 0; i < lhswf.ncols(); ++i) {
    Column rescol = evaluate1(lhswf.retrieve_column(i),
                              rhswf.retrieve_column(i));
    outputs.add_column(std::move(rescol), std::string(), gmode);
  }
  return outputs;
}


std::string FExpr_BinaryOp::repr() const {
  auto lstr = lhs_->repr();
  auto rstr = rhs_->repr();
  if (lhs_->precedence() < precedence()) {
    lstr = "(" + lstr + ")";
  }
  if (rhs_->precedence() <= precedence()) {
    rstr = "(" + rstr + ")";
  }
  return lstr + " " + name() + " " + rstr;
}




}}  // namespace dt::expr
