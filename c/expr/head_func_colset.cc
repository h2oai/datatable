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


Workframe Head_Func_Colset::evaluate_n(const vecExpr& args, EvalContext& ctx) const {
  xassert(args.size() == 2);
  Workframe lhs = args[0].evaluate_n(ctx);
  Workframe rhs = args[1].evaluate_n(ctx);
  return (op == Op::SETPLUS)? _extend(std::move(lhs), std::move(rhs))
                            : _remove(std::move(lhs), std::move(rhs));
  // for (auto& out : outputs.get_items()) {
  //   const auto& info = unary_library.get_infox(op, out.column.stype());
  //   if (info.cast_stype != SType::VOID) {
  //     out.column.cast_inplace(info.cast_stype);
  //   }
  //   out.column = info.vcolfn(std::move(out.column));
  // }
  // return outputs;
}


Workframe Head_Func_Colset::_extend(Workframe&& lhs, Workframe&& rhs) const {
  Workframe outputs(lhs.get_context());
  outputs.cbind(std::move(lhs));
  outputs.cbind(std::move(rhs));
  return outputs;
}


Workframe Head_Func_Colset::_remove(Workframe&& lhs, Workframe&& rhs) const {
  (void) rhs;
  return std::move(lhs);
}




}}  // namespace dt::expr
