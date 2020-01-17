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
#include "column/range.h"
#include "expr/head_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {



Head_Literal_Range::Head_Literal_Range(py::orange x)
  : value(x) {}


Kind Head_Literal_Range::get_expr_kind() const {
  return Kind::SliceInt;
}



Workframe Head_Literal_Range::evaluate_n(
    const vecExpr&, EvalContext& ctx, bool) const
{
  Workframe out(ctx);
  out.add_column(Column(new Range_ColumnImpl(value.start(),
                                             value.stop(),
                                             value.step())),
                 "", Grouping::GtoALL);
  return out;
}



Workframe Head_Literal_Range::evaluate_f(
    EvalContext& ctx, size_t frame_id, bool) const
{
  size_t len = ctx.get_datatable(frame_id)->ncols();
  size_t start, count, step;
  bool ok = value.normalize(len, &start, &count, &step);
  if (!ok) {
    throw ValueError() << _repr_range() << " cannot be applied to a Frame with "
        << len << " columns";
  }
  Workframe outputs(ctx);
  for (size_t i = 0; i < count; ++i) {
    outputs.add_ref_column(frame_id, start + i * step);
  }
  return outputs;
}


Workframe Head_Literal_Range::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool allow_new) const
{
  return evaluate_f(ctx, 0, allow_new);
}


RowIndex Head_Literal_Range::evaluate_i(const vecExpr&, EvalContext& ctx) const {
  size_t len = ctx.nrows();
  size_t start, count, step;
  bool ok = value.normalize(len, &start, &count, &step);
  if (!ok) {
    throw ValueError() << _repr_range() << " cannot be applied to a Frame with "
        << len << " row" << ((len == 1)? "" : "s");
  }
  return RowIndex(start, count, step);
}


RiGb Head_Literal_Range::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw NotImplError() << "A range selector cannot yet be used in i in the "
                          "presence of by clause";
}



std::string Head_Literal_Range::_repr_range() const {
  auto start = value.start();
  auto stop  = value.stop();
  auto step  = value.step();
  std::string res = "range(";
  if (start != 0 || step != 1) {
    res += std::to_string(start) + ", ";
  }
  res += std::to_string(stop);
  if (step != 1) {
    res += ", " + std::to_string(step);
  }
  res += ')';
  return res;
}



}}  // namespace dt::expr
