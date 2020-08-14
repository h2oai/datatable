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
#include "expr/eval_context.h"
#include "expr/fexpr_literal.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


FExpr_Literal_SliceStr::FExpr_Literal_SliceStr(py::oslice x)
  : start_(x.start_obj()),
    end_(x.stop_obj())
{}


//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Literal_SliceStr::evaluate_f(EvalContext& ctx, size_t ns) const
{
  DataTable* df = ctx.get_datatable(ns);
  size_t istart = start_.is_none()? 0 : df->xcolindex(start_);
  size_t iend = end_.is_none()? df->ncols() - 1 : df->xcolindex(end_);

  Workframe outputs(ctx);
  size_t di = (istart <= iend)? 1 : size_t(-1);
  for (size_t i = istart; ; i += di) {
    outputs.add_ref_column(ns, i);
    if (i == iend) break;
  }
  return outputs;
}


Workframe FExpr_Literal_SliceStr::evaluate_j(EvalContext& ctx) const {
  return evaluate_f(ctx, 0);
}


RowIndex FExpr_Literal_SliceStr::evaluate_i(EvalContext&) const {
  throw TypeError() << "A string slice cannot be used as a row selector";
}


RiGb FExpr_Literal_SliceStr::evaluate_iby(EvalContext&) const {
  throw TypeError() << "A string slice cannot be used as a row selector";
}




//------------------------------------------------------------------------------
// Other methods
//------------------------------------------------------------------------------

Kind FExpr_Literal_SliceStr::get_expr_kind() const {
  return Kind::SliceStr;
}


std::string FExpr_Literal_SliceStr::repr() const {
  std::string out;
  if (!start_.is_none()) {
    out += start_.repr().to_string();
  }
  out += ':';
  if (!end_.is_none()) {
    out += end_.repr().to_string();
  }
  return out;
}




}}  // namespace dt::expr
