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
#include "expr/outputs.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


Head_Func_Binary::Head_Func_Binary(Op op_) : op(op_) {}


Outputs Head_Func_Binary::evaluate(const vecExpr& args, workframe& wf) const {
  xassert(args.size() == 2);
  Outputs lhs = args[0].evaluate(wf);
  Outputs rhs = args[1].evaluate(wf);
  size_t lmask = (lhs.size() == 1)? 0 : size_t(-1);
  size_t rmask = (rhs.size() == 1)? 0 : size_t(-1);
  if (lmask && rmask && lhs.size() != rhs.size()) {
    throw ValueError() << "Incompatible column vectors in a binary operation: "
      "LHS contains " << lhs.size() << " items, while RHS has " << rhs.size()
      << " items";
  }
  size_t size = lmask? lhs.size() : rmask? rhs.size() : 1;
  size_t lhs_grouping_level = lhs.get_grouping_level();
  size_t rhs_grouping_level = rhs.get_grouping_level();
  if (lhs_grouping_level < rhs_grouping_level) {
    lhs.increase_grouping_level(rhs_grouping_level, wf);
    // lhs_grouping_level = lhs.get_grouping_level();
  }
  if (rhs_grouping_level < lhs_grouping_level) {
    rhs.increase_grouping_level(lhs_grouping_level, wf);
  }
  Outputs outputs;
  for (size_t i = 0; i < size; ++i) {
    outputs.add(binaryop(op, lhs.get_column(i & lmask),
                             rhs.get_column(i & rmask)),
                std::move(lhs.get_name(i & lmask)),  // name is inherited from the first argument
                lhs_grouping_level);  // grouping levels are same
  }
  return outputs;
}




}}  // namespace dt::expr
