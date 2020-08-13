//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "expr/fexpr_column.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "utils/assert.h"
namespace dt {
namespace expr {


FExpr_ColumnAsAttr::FExpr_ColumnAsAttr(size_t ns, py::robj pyname)
  : namespace_(ns),
    pyname_(pyname)
{
  xassert(pyname_.is_string());
}


Workframe FExpr_ColumnAsAttr::evaluate_n(EvalContext& ctx) const {
  if (namespace_ >= ctx.nframes()) {
    throw ValueError()
        << "Column expression references a non-existing join frame";
  }
  auto df = ctx.get_datatable(namespace_);
  size_t j = df->xcolindex(pyname_);
  Workframe out(ctx);
  out.add_ref_column(namespace_, j);
  return out;
}


int FExpr_ColumnAsAttr::precedence() const noexcept {
  return 16;
}


std::string FExpr_ColumnAsAttr::repr() const {
  return (namespace_ == 0? "f." : "g.") + pyname_.to_string();
}



py::oobj FExpr_ColumnAsAttr::get_pyname() const {
  return pyname_;
}




}}  // namespace dt::expr
