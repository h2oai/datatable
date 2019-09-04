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
#include "expr/head_literal.h"
#include "expr/outputs.h"
namespace dt {
namespace expr {


Head_Literal_SliceStr::Head_Literal_SliceStr(py::oslice x)
  : start(x.start_obj()),
    end(x.stop_obj()) {}


Head::Kind Head_Literal_SliceStr::get_expr_kind() const {
  return Head::Kind::SliceStr;
}



Outputs Head_Literal_SliceStr::evaluate(const vecExpr&, workframe&) const {
  throw TypeError() << "A slice expression cannot appear in this context";
}



Outputs Head_Literal_SliceStr::evaluate_f(workframe& wf, size_t frame_id) const
{
  DataTable* df = wf.get_datatable(frame_id);
  size_t istart = start.is_none()? 0 : df->xcolindex(start);
  size_t iend = end.is_none()? df->ncols - 1 : df->xcolindex(end);

  Outputs res;
  size_t di = (istart <= iend)? 1 : size_t(-1);
  for (size_t i = istart; ; i += di) {
    res.add_column(df, i);
    if (i == iend) break;
  }
  return res;
}




}}  // namespace dt::expr
