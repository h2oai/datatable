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
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/outputs.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// count()
//------------------------------------------------------------------------------

static Column _count0(workframe& wf)
{
  if (wf.has_groupby()) {
    // TODO: convert this into a virtual column
    const Groupby& grpby = wf.get_groupby();
    size_t ng = grpby.ngroups();
    const int32_t* offsets = grpby.offsets_r();
    Column col = Column::new_data_column(SType::INT64, ng);
    auto d_res = static_cast<int64_t*>(col->data_w());
    for (size_t i = 0; i < ng; ++i) {
      d_res[i] = offsets[i + 1] - offsets[i];
    }
    return col;
  }
  else {
    auto value = static_cast<int64_t>(wf.nrows());
    return Const_ColumnImpl::make_int_column(1, value);
  }
}




//------------------------------------------------------------------------------
// Head_Reduce_Nullary
//------------------------------------------------------------------------------

static Outputs _wrap_column(workframe& wf, Column&& col, std::string&& name) {
  Outputs outputs(wf);
  outputs.add(std::move(col), std::move(name), Grouping::GtoONE);
  return outputs;
}


Outputs Head_Reduce_Nullary::evaluate_n(const vecExpr& args, workframe& wf) const
{
  xassert(args.size() == 0);
  (void) args;
  switch (op) {
    case Op::COUNT0: return _wrap_column(wf, _count0(wf), "count");
    default: break;
  }
  throw RuntimeError() << "Unknown op " << static_cast<size_t>(op)
                       << " in Head_Reduce_Nullary";
}




}}  // namespace dt::expr
