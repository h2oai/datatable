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




}}  // namespace dt::expr
