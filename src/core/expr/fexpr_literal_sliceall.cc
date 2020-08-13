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
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

// `f[:]` will return all columns from `f`
//
Workframe FExpr_Literal_SliceAll::evaluate_f(
    EvalContext& ctx, size_t frame_id) const
{
  size_t ncols = ctx.get_datatable(frame_id)->ncols();
  Workframe outputs(ctx);
  for (size_t i = 0; i < ncols; ++i) {
    if (ctx.has_group_column(frame_id, i)) continue;
    outputs.add_ref_column(frame_id, i);
  }
  return outputs;
}


// When `:` is used in j expression, it means "all columns in all frames,
// including the joined frames". There are 2 exceptions though:
//   - any groupby columns are not added (since they should be added at the
//     front by the groupby operation itself);
//   - key columns in naturally joined frames are skipped, to avoid duplication.
//
Workframe FExpr_Literal_SliceAll::evaluate_j(EvalContext& ctx) const {
  Workframe outputs(ctx);
  for (size_t i = 0; i < ctx.nframes(); ++i) {
    const DataTable* dti = ctx.get_datatable(i);
    size_t j0 = ctx.is_naturally_joined(i)? dti->nkeys() : 0;
    for (size_t j = j0; j < dti->ncols(); ++j) {
      if (ctx.has_group_column(i, j)) continue;
      outputs.add_ref_column(i, j);
    }
  }
  return outputs;
}


// When `:` is used as i-node, it means all rows are selected.
RowIndex FExpr_Literal_SliceAll::evaluate_i(EvalContext&) const {
  return RowIndex();
}


RiGb FExpr_Literal_SliceAll::evaluate_iby(EvalContext&) const {
  return std::make_pair(RowIndex(), Groupby());
}


//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

std::string FExpr_Literal_SliceAll::repr() const {
  return ":";
}


Kind FExpr_Literal_SliceAll::get_expr_kind() const {
  return Kind::SliceAll;
}




}}  // namespace dt::expr
