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
#include "column/const.h"
#include "expr/head_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


Head_Literal_Float::Head_Literal_Float(double x) : value(x) {}

Kind Head_Literal_Float::get_expr_kind() const {
  return Kind::Float;
}



Workframe Head_Literal_Float::evaluate_n(const vecExpr&, EvalContext& ctx) const {
  return _wrap_column(ctx, Const_ColumnImpl::make_float_column(1, value));
}


// A float value is assigned to a DT[i,j] expression:
//
//   DT[:, j] = -1
//
// The `j` columns must be float.
//
Workframe Head_Literal_Float::evaluate_r(
    const vecExpr&, EvalContext& ctx, const std::vector<SType>& stypes) const
{
  Workframe outputs(ctx);
  for (SType stype : stypes) {
    if (!(stype == SType::FLOAT32 || stype == SType::FLOAT64)) {
      // Either type mismatch (the caller will throw an error then),
      // or stype is VOID, in which case we default to FLOAT64
      stype = SType::FLOAT64;
    }
    outputs.add_column(
        Const_ColumnImpl::make_float_column(1, value, stype),
        "", Grouping::SCALAR);
  }
  return outputs;
}


Workframe Head_Literal_Float::evaluate_f(EvalContext&, size_t, bool) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "column selector";
}


Workframe Head_Literal_Float::evaluate_j(const vecExpr&, EvalContext&, bool) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "column selector";
}


RowIndex Head_Literal_Float::evaluate_i(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "row selector";
}


RiGb Head_Literal_Float::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A floating-point value cannot be used as a "
                       "row selector";
}




}}  // namespace dt::expr
