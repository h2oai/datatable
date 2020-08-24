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
#include "expr/eval_context.h"
#include "expr/fexpr_func_unary.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


FExpr_FuncUnary::FExpr_FuncUnary(ptrExpr&& arg)
  : arg_(std::move(arg)) {}


std::string FExpr_FuncUnary::repr() const {
  std::string out = name();
  out += '(';
  out += arg_->repr();
  out += ')';
  return out;
}


Workframe FExpr_FuncUnary::evaluate_n(EvalContext& ctx) const {
  Workframe wf = arg_->evaluate_n(ctx);
  for (size_t i = 0; i < wf.ncols(); ++i) {
    Column col = wf.retrieve_column(i);
    wf.replace_column(i, evaluate1(std::move(col)));
  }
  return wf;
}




}}  // namespace dt::expr
