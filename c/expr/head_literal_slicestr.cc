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


Head_Literal_SliceStr::Head_Literal_SliceStr(py::oslice x)
  : start(x.start_obj()),
    end(x.stop_obj()) {}


Kind Head_Literal_SliceStr::get_expr_kind() const {
  return Kind::SliceStr;
}



Workframe Head_Literal_SliceStr::evaluate_n(
    const vecExpr&, EvalContext&, bool) const
{
  throw TypeError() << "A slice expression cannot appear in this context";
}


// Ignore the `allow_new` flag (else how would we expand the slice
// into a list?)
//
Workframe Head_Literal_SliceStr::evaluate_f(
    EvalContext& ctx, size_t frame_id, bool) const
{
  DataTable* df = ctx.get_datatable(frame_id);
  size_t istart = start.is_none()? 0 : df->xcolindex(start);
  size_t iend = end.is_none()? df->ncols() - 1 : df->xcolindex(end);

  Workframe outputs(ctx);
  size_t di = (istart <= iend)? 1 : size_t(-1);
  for (size_t i = istart; ; i += di) {
    outputs.add_ref_column(frame_id, i);
    if (i == iend) break;
  }
  return outputs;
}


Workframe Head_Literal_SliceStr::evaluate_j(
    const vecExpr&, EvalContext& ctx, bool allow_new) const
{
  return evaluate_f(ctx, 0, allow_new);
}


RowIndex Head_Literal_SliceStr::evaluate_i(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A string slice cannot be used as a row selector";
}


RiGb Head_Literal_SliceStr::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A string slice cannot be used as a row selector";
}





}}  // namespace dt::expr
