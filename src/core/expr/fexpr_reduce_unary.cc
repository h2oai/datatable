//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#include "expr/eval_context.h"
#include "expr/fexpr_reduce_unary.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {

FExpr_ReduceUnary::FExpr_ReduceUnary(ptrExpr&& arg)
  : arg_(std::move(arg)) {}


Workframe FExpr_ReduceUnary::evaluate_n(EvalContext &ctx) const {
  Workframe outputs(ctx);
  Workframe wf = arg_->evaluate_n(ctx);
  // If there is no `by()` in the context, `gby` is
  // a single-group-all-rows Groupby.
  Groupby gby = ctx.get_groupby();

  // Check if the input frame is grouped as `GtoONE`
  bool is_wf_grouped = (wf.get_grouping_mode() == Grouping::GtoONE);

  if (is_wf_grouped) {
    // Check if the input frame columns are grouped
    bool are_cols_grouped = ctx.has_group_column(
                              wf.get_frame_id(0),
                              wf.get_column_id(0)
                            );

    if (!are_cols_grouped) {
      // When the input frame is `GtoONE`, but columns are not grouped,
      // it means we are dealing with the output of another reducer.
      // In such a case we create a new groupby, that has one element
      // per a group. This may not be optimal performance-wise,
      // but chained reducers is a very rare scenario.
      xassert(gby.size() == wf.nrows());
      gby = Groupby::nrows_groups(gby.size());
    }
  }

  for (size_t i = 0; i < wf.ncols(); ++i) {
    Column coli = wf.retrieve_column(i);
    coli = evaluate1(std::move(coli), gby, is_wf_grouped);
    outputs.add_column(std::move(coli), wf.retrieve_name(i), Grouping::GtoONE);
  }

  return outputs;
}


std::string FExpr_ReduceUnary::repr() const {
  std::string out = name();
  out += '(';
  out += arg_->repr();
  out += ')';
  return out;
}


}}  // namespace dt::expr
