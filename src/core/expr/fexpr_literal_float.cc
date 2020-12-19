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
#include "column/const.h"
#include "expr/fexpr_literal.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "ltype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Literal_Float::FExpr_Literal_Float(double x)
  : value_(x) {}


ptrExpr FExpr_Literal_Float::make(py::robj src) {
  double x = src.to_double();
  return ptrExpr(new FExpr_Literal_Float(x));
}



//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Literal_Float::evaluate_n(EvalContext& ctx) const {
  return Workframe(ctx, Const_ColumnImpl::make_float_column(1, value_));
}


// A float value is assigned to a DT[i,j] expression:
//
//   DT[:, j] = -1
//
// The `j` columns must be float.
//
Workframe FExpr_Literal_Float::evaluate_r(
    EvalContext& ctx, const sztvec& indices) const
{
  auto dt0 = ctx.get_datatable(0);

  Workframe outputs(ctx);
  for (size_t i : indices) {
    SType stype;
    if (i < dt0->ncols()) {
      const Column& col = dt0->get_column(i);
      stype = (col.ltype() == LType::REAL)? col.stype() : SType::FLOAT64;
    } else {
      stype = SType::AUTO;
    }

    outputs.add_column(
        Const_ColumnImpl::make_float_column(1, value_, stype),
        "", Grouping::SCALAR);
  }
  return outputs;
}


Workframe FExpr_Literal_Float::evaluate_f(EvalContext&, size_t) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "column selector";
}


Workframe FExpr_Literal_Float::evaluate_j(EvalContext&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "column selector";
}


RowIndex FExpr_Literal_Float::evaluate_i(EvalContext&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "row selector";
}


RiGb FExpr_Literal_Float::evaluate_iby(EvalContext&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "row selector";
}



//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_Float::get_expr_kind() const {
  return Kind::Float;
}


int FExpr_Literal_Float::precedence() const noexcept {
  return 18;
}


std::string FExpr_Literal_Float::repr() const {
  return std::to_string(value_);
}




}}  // namespace dt::expr
