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
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


Head_Func_Colset::Head_Func_Colset(Op op_) : op(op_) {
  xassert(op == Op::SETPLUS || op == Op::SETMINUS);
}


Workframe Head_Func_Colset::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 2);
  if (op == Op::SETPLUS) {
    Workframe lhs = args[0].evaluate_n(ctx);
    Workframe rhs = args[1].evaluate_n(ctx);
    lhs.cbind(std::move(rhs));
    return lhs;
  }
  else {
    Workframe lhs = args[0].evaluate_n(ctx);
    // RHS must be evaluated with `allow_new = true`, so that removing
    // a column which is not in the frame would not throw an error.
    Workframe rhs = args[1].evaluate_n(ctx, true);
    lhs.remove(rhs);
    return lhs;
  }
}




}}  // namespace dt::expr
