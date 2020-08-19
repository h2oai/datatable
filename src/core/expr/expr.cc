//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "expr/head.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "expr/eval_context.h"
#include "datatable.h"
#include "datatablemodule.h"
namespace dt {
namespace expr {




//------------------------------------------------------------------------------
// Expr construction
//------------------------------------------------------------------------------

OldExpr::OldExpr(py::robj src)
{
  xassert(src.is_dtexpr());

  auto op     = src.get_attr("_op").to_size_t();
  auto args   = src.get_attr("_args").to_otuple();
  auto params = src.get_attr("_params").to_otuple();

  for (size_t i = 0; i < args.size(); ++i) {
    inputs.emplace_back(as_fexpr(args[i]));
  }
  head = Head_Func::from_op(static_cast<Op>(op), params);
}



//------------------------------------------------------------------------------
// Expr core functionality
//------------------------------------------------------------------------------

Kind OldExpr::get_expr_kind() const {
  return head->get_expr_kind();
}

OldExpr::operator bool() const noexcept {
  return bool(head);
}


Workframe OldExpr::evaluate_n(EvalContext& ctx) const {
  return head->evaluate_n(inputs, ctx);
}


Workframe OldExpr::evaluate_j(EvalContext& ctx) const
{
  return head->evaluate_j(inputs, ctx);
}

Workframe OldExpr::evaluate_r(EvalContext& ctx, const sztvec& indices) const
{
  return head->evaluate_r(inputs, ctx, indices);
}


Workframe OldExpr::evaluate_f(
    EvalContext& ctx, size_t frame_id) const
{
  return head->evaluate_f(ctx, frame_id);
}


RowIndex OldExpr::evaluate_i(EvalContext& ctx) const {
  return head->evaluate_i(inputs, ctx);
}


void OldExpr::prepare_by(EvalContext& ctx, Workframe& wf,
                      std::vector<SortFlag>& flags) const
{
  head->prepare_by(inputs, ctx, wf, flags);
}


RiGb OldExpr::evaluate_iby(EvalContext& ctx) const {
  return head->evaluate_iby(inputs, ctx);
}



std::shared_ptr<FExpr> OldExpr::unnegate_column() const {
  auto unaryfn_head = dynamic_cast<Head_Func_Unary*>(head.get());
  if (unaryfn_head && unaryfn_head->get_op() == Op::UMINUS) {
    xassert(inputs.size() == 1);
    return inputs[0];
  }
  return nullptr;
}


int OldExpr::precedence() const noexcept {
  return 0;
}

std::string OldExpr::repr() const {
  return "?";
}



}}  // namespace dt::expr
