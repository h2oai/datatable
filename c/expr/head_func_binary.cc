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
#include <string>
#include "expr/fbinary/bimaker.h"
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


Head_Func_Binary::Head_Func_Binary(Op op_) : op(op_) {}


Workframe Head_Func_Binary::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 2);
  Workframe lhs = args[0].evaluate_n(ctx);
  Workframe rhs = args[1].evaluate_n(ctx);
  if (lhs.ncols() == 1) lhs.repeat_column(rhs.ncols());
  if (rhs.ncols() == 1) rhs.repeat_column(lhs.ncols());
  if (lhs.ncols() != rhs.ncols()) {
    throw ValueError() << "Incompatible column vectors in a binary operation: "
      "LHS contains " << lhs.ncols() << " items, while RHS has " << rhs.ncols()
      << " items";
  }
  lhs.sync_grouping_mode(rhs);
  auto gmode = lhs.get_grouping_mode();
  Workframe outputs(ctx);
  for (size_t i = 0; i < lhs.ncols(); ++i) {
    Column lhscol = lhs.retrieve_column(i);
    Column rhscol = rhs.retrieve_column(i);
    Column rescol = binaryop(op, std::move(lhscol), std::move(rhscol));
    outputs.add_column(std::move(rescol), std::string(), gmode);
  }
  return outputs;
}




}}  // namespace dt::expr
