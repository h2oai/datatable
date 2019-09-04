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
#include "column/column_const.h"
#include "expr/head_literal.h"
#include "expr/outputs.h"
namespace dt {
namespace expr {


Head_Literal_Float::Head_Literal_Float(double x) : value(x) {}

Head::Kind Head_Literal_Float::get_expr_kind() const {
  return Head::Kind::Float;
}



Outputs Head_Literal_Float::evaluate(const vecExpr&, workframe&) const {
  return _wrap_column(Const_ColumnImpl::make_float_column(1, value));
}


Outputs Head_Literal_Float::evaluate_f(workframe&, size_t) const {
  throw TypeError() << "A floating-point value cannot be used as a "
      "column selector";
}


Outputs Head_Literal_Float::evaluate_j(const vecExpr&, workframe&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
      "column selector";
}



}}  // namespace dt::expr
