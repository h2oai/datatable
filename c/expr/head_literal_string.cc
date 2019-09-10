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




Head_Literal_String::Head_Literal_String(py::robj x) : pystr(x) {}

Kind Head_Literal_String::get_expr_kind() const {
  return Kind::Str;
}


Outputs Head_Literal_String::evaluate(const vecExpr&, workframe& wf) const {
  return _wrap_column(wf,
            Const_ColumnImpl::make_string_column(1, pystr.to_string()));
}


Outputs Head_Literal_String::evaluate_f(workframe& wf, size_t frame_id) const
{
  auto df = wf.get_datatable(frame_id);
  size_t j = df->xcolindex(pystr);
  Outputs outputs(wf);
  outputs.add_column(frame_id, j);
  return outputs;
}



Outputs Head_Literal_String::evaluate_j(const vecExpr&, workframe& wf) const {
  return evaluate_f(wf, 0);
}




}}  // namespace dt::expr
