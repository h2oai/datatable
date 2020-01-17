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
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// count()
//------------------------------------------------------------------------------

static Column _count0(EvalContext& ctx)
{
  if (ctx.has_groupby()) {
    // TODO: convert this into a virtual column
    const Groupby& grpby = ctx.get_groupby();
    size_t ng = grpby.size();
    const int32_t* offsets = grpby.offsets_r();
    Column col = Column::new_data_column(ng, SType::INT64);
    auto d_res = static_cast<int64_t*>(col.get_data_editable());
    for (size_t i = 0; i < ng; ++i) {
      d_res[i] = offsets[i + 1] - offsets[i];
    }
    return col;
  }
  else {
    auto value = static_cast<int64_t>(ctx.nrows());
    return Const_ColumnImpl::make_int_column(1, value);
  }
}




//------------------------------------------------------------------------------
// Head_Reduce_Nullary
//------------------------------------------------------------------------------

static Workframe _wrap_column(EvalContext& ctx, Column&& col, std::string&& name) {
  Workframe outputs(ctx);
  outputs.add_column(std::move(col), std::move(name), Grouping::GtoONE);
  return outputs;
}


Workframe Head_Reduce_Nullary::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 0);
  (void) args;
  switch (op) {
    case Op::COUNT0: return _wrap_column(ctx, _count0(ctx), "count");
    default: break;
  }
  throw RuntimeError() << "Unknown op " << static_cast<size_t>(op)
                       << " in Head_Reduce_Nullary";
}




}}  // namespace dt::expr
