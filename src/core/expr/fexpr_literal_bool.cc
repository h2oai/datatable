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
#include "expr/eval_context.h"
#include "expr/fexpr_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Literal_Bool::FExpr_Literal_Bool(bool x)
  : value_(x) {}


ptrExpr FExpr_Literal_Bool::make(py::robj src) {
  int8_t t = src.to_bool_force();
  xassert(t == 0 || t == 1);
  return ptrExpr(new FExpr_Literal_Bool(static_cast<bool>(t)));
}




//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Literal_Bool::evaluate_n(EvalContext& ctx) const {
  return Workframe(ctx, Const_ColumnImpl::make_bool_column(1, value_));
}


// A boolean value is used as a replacement target. This is valid
// only if `j` column(s) have stype BOOL.
//
//   DT[:, j] = True
//
Workframe FExpr_Literal_Bool::evaluate_r(EvalContext& ctx, const sztvec&) const
{
  return Workframe(ctx, Const_ColumnImpl::make_bool_column(1, value_));
}



Workframe FExpr_Literal_Bool::evaluate_f(EvalContext&, size_t) const {
  throw TypeError()
    << "A boolean value cannot be used as a column selector";
}


Workframe FExpr_Literal_Bool::evaluate_j(EvalContext&) const {
  throw TypeError()
    << "A boolean value cannot be used as a column selector";
}


RowIndex FExpr_Literal_Bool::evaluate_i(EvalContext&) const {
  throw TypeError() << "A boolean value cannot be used as a row selector";
}


RiGb FExpr_Literal_Bool::evaluate_iby(EvalContext&) const {
  throw TypeError() << "A boolean value cannot be used as a row selector";
}




//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_Bool::get_expr_kind() const {
  return Kind::Bool;
}


bool FExpr_Literal_Bool::evaluate_bool() const {
  return value_;
}


int FExpr_Literal_Bool::precedence() const noexcept {
  return 18;
}


std::string FExpr_Literal_Bool::repr() const {
  return value_? "True" : "False";
}




}}  // namespace dt::expr
