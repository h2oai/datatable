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



Head_Literal_Int::Head_Literal_Int(int64_t x) : value(x) {}

Kind Head_Literal_Int::get_expr_kind() const {
  return Kind::Int;
}

int64_t Head_Literal_Int::get_value() const {
  return value;
}




Workframe Head_Literal_Int::evaluate_n(const vecExpr&, EvalContext& ctx) const {
  return _wrap_column(ctx, Const_ColumnImpl::make_int_column(1, value));
}



Workframe Head_Literal_Int::evaluate_f(
    EvalContext& ctx, size_t frame_id, bool allow_new) const
{
  auto df = ctx.get_datatable(frame_id);
  Workframe outputs(ctx);
  int64_t icols = static_cast<int64_t>(df->ncols());
  if (value < -icols || value >= icols) {
    if (!(allow_new && value > 0)) {
      throw ValueError()
          << "Column index `" << value << "` is invalid for a Frame with "
          << icols << " column" << (icols == 1? "" : "s");
    }
    outputs.add_placeholder(std::string(), frame_id);
  } else {
    size_t i = static_cast<size_t>(value < 0? value + icols : value);
    outputs.add_ref_column(frame_id, i);
  }
  return outputs;
}



Workframe Head_Literal_Int::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool allow_new) const
{
  return evaluate_f(ctx, 0, allow_new);
}



// An integer value is assigned to a DT[i,j] expression:
//
//   DT[:, j] = -1
//
// This is allowed provided that the columns in `j` are either
// integer or float.
//
Workframe Head_Literal_Int::evaluate_r(
    const vecExpr&, EvalContext& ctx, const std::vector<SType>& stypes) const
{
  Workframe outputs(ctx);
  for (SType stype : stypes) {
    LType ltype = ::info(stype).ltype();
    Column newcol;
    if (ltype == LType::INT) {
      // This creates a column with the requested `stype`, but only
      // if the `value` fits inside the range of that stype. If not,
      // the column will be auto-promoted to the next smallest integer
      // stype.
      newcol = Const_ColumnImpl::make_int_column(1, value, stype);
    }
    else if (ltype == LType::REAL) {
      newcol = Const_ColumnImpl::make_float_column(1, value);
    }
    else {
      throw TypeError() << "An integer value cannot be assigned to a column "
                           "of stype `" << stype << "`";
    }
    outputs.add_column(std::move(newcol), std::string(), Grouping::SCALAR);
  }
  return outputs;
}



RowIndex Head_Literal_Int::evaluate_i(const vecExpr&, EvalContext& ctx) const {
  int64_t inrows = static_cast<int64_t>(ctx.nrows());
  if (value < -inrows || value >= inrows) {
    throw ValueError() << "Row `" << value << "` is invalid for a frame with "
        << inrows << " row" << (inrows == 1? "" : "s");
  }
  auto irow = static_cast<size_t>((value >= 0)? value : value + inrows);
  return RowIndex(irow, 1, 1);
}


RiGb Head_Literal_Int::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw NotImplError() << "Head_Literal_Int::evaluate_iby() not implemented yet";
}




}}  // namespace dt::expr
