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


Kind Head_Literal_None::get_expr_kind() const {
  return Kind::None;
}


Workframe Head_Literal_None::evaluate_n(
    const vecExpr&, EvalContext& ctx, bool) const
{
  return _wrap_column(ctx, Const_ColumnImpl::make_na_column(1));
}



// When used as j, `None` means select all columns
Workframe Head_Literal_None::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool) const
{
  size_t n = ctx.get_datatable(0)->ncols();
  Workframe outputs(ctx);
  for (size_t i = 0; i < n; ++i) {
    outputs.add_ref_column(0, i);
  }
  return outputs;
}


// None value is used as a replacement target:
//
//  DT[:, j] = None
//
// In this case we replace columns in `j` with NA columns, but keep
// their stypes.
//
Workframe Head_Literal_None::evaluate_r(
    const vecExpr&, EvalContext& ctx, const intvec& indices) const
{
  auto dt0 = ctx.get_datatable(0);
  Workframe outputs(ctx);
  for (size_t i : indices) {
    // At some point in the future we may allow VOID columns to be created too
    SType stype = (i < dt0->ncols())? dt0->get_column(i).stype()
                                    : SType::BOOL;
    outputs.add_column(
      Column(new ConstNa_ColumnImpl(1, stype)),
      std::string(),
      Grouping::SCALAR
    );
  }
  return outputs;
}



// When used in f, `None` means select nothing
Workframe Head_Literal_None::evaluate_f(EvalContext& ctx, size_t, bool) const {
  return Workframe(ctx);
}


// When used in i, `None` means select all rows
RowIndex Head_Literal_None::evaluate_i(const vecExpr&, EvalContext&) const {
  return RowIndex();
}


RiGb Head_Literal_None::evaluate_iby(const vecExpr&, EvalContext& ctx) const {
  return std::make_pair(RowIndex(), ctx.get_groupby());
}




}}  // namespace dt::expr
