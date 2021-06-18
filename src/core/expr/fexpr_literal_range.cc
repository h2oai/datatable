//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "column/range.h"
#include "expr/eval_context.h"
#include "expr/fexpr_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Literal_Range::FExpr_Literal_Range(py::orange x)
  : value_(x) {}


ptrExpr FExpr_Literal_Range::make(py::robj src) {
  py::orange rr = src.to_orange();
  return ptrExpr(new FExpr_Literal_Range(rr));
}




//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Literal_Range::evaluate_n(EvalContext& ctx) const {
  Workframe out(ctx);
  out.add_column(Column(new Range_ColumnImpl(value_.start(),
                                             value_.stop(),
                                             value_.step(), dt::Type())),
                 "", Grouping::GtoALL);
  return out;
}


Workframe FExpr_Literal_Range::evaluate_r(EvalContext& ctx, const sztvec&) const
{
  return evaluate_n(ctx);
}


Workframe FExpr_Literal_Range::evaluate_f(EvalContext& ctx, size_t ns) const {
  size_t len = ctx.get_datatable(ns)->ncols();
  size_t start, count, step;
  bool ok = value_.normalize(len, &start, &count, &step);
  if (!ok) {
    throw ValueError() << repr() << " cannot be applied to a Frame with "
        << len << " columns";
  }
  Workframe outputs(ctx);
  for (size_t i = 0; i < count; ++i) {
    outputs.add_ref_column(ns, start + i * step);
  }
  return outputs;
}


Workframe FExpr_Literal_Range::evaluate_j(EvalContext& ctx) const {
  return evaluate_f(ctx, 0);
}


RowIndex FExpr_Literal_Range::evaluate_i(EvalContext& ctx) const {
  size_t len = ctx.nrows();
  size_t start, count, step;
  bool ok = value_.normalize(len, &start, &count, &step);
  if (!ok) {
    throw ValueError() << repr() << " cannot be applied to a Frame with "
        << len << " row" << ((len == 1)? "" : "s");
  }
  return RowIndex(start, count, step);
}


RiGb FExpr_Literal_Range::evaluate_iby(EvalContext&) const {
  throw NotImplError() << "A range selector cannot yet be used in i in the "
                          "presence of by clause";
}




//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_Range::get_expr_kind() const {
  return Kind::SliceInt;
}


int FExpr_Literal_Range::precedence() const noexcept {
  return 16;
}


std::string FExpr_Literal_Range::repr() const {
  return value_.repr().to_string();
}




}}  // namespace dt::expr
