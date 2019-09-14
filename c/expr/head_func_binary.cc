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
#include "expr/expr_binaryop.h"  // TODO: merge into this file
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


Head_Func_Binary::Head_Func_Binary(Op op_) : op(op_) {}


Workframe Head_Func_Binary::evaluate_n(const vecExpr& args, EvalContext& ctx) const {
  xassert(args.size() == 2);
  Workframe lhs = args[0].evaluate_n(ctx);
  Workframe rhs = args[1].evaluate_n(ctx);
  size_t lmask = (lhs.size() == 1)? 0 : size_t(-1);
  size_t rmask = (rhs.size() == 1)? 0 : size_t(-1);
  if (lmask && rmask && lhs.size() != rhs.size()) {
    throw ValueError() << "Incompatible column vectors in a binary operation: "
      "LHS contains " << lhs.size() << " items, while RHS has " << rhs.size()
      << " items";
  }
  size_t size = lmask? lhs.size() : rmask? rhs.size() : 1;
  lhs.sync_grouping_mode(rhs);
  auto gmode = lhs.get_grouping_mode();
  Workframe outputs(ctx);
  for (size_t i = 0; i < size; ++i) {
    Column lhscol = lhs.retrieve_column(i & lmask);
    Column rhscol = rhs.retrieve_column(i & rmask);
    outputs.add_column(binaryop(op, lhscol, rhscol),
                       std::string(),
                       gmode);
  }
  return outputs;
}




}}  // namespace dt::expr
