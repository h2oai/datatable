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


FExpr_ColumnAsArg::FExpr_ColumnAsArg(size_t ns, py::robj arg) {
  namespace_ = ns;
  arg_ = as_fexpr(arg);
}


Workframe FExpr_ColumnAsArg::evaluate_n(EvalContext& ctx) const {
  if (namespace_ >= ctx.nframes()) {
    throw ValueError()
        << "Column expression references a non-existing join frame";
  }
  // TODO: perhaps all `evaluate_f()` functionality should be brought here?
  return arg_->evaluate_f(ctx, namespace_);
}


int FExpr_ColumnAsArg::precedence() const noexcept {
  return 16;
}


std::string FExpr_ColumnAsArg::repr() const {
  return (namespace_ == 0? "f[" : "g[") + arg_->repr() + ']';
}



ptrExpr FExpr_ColumnAsArg::get_arg() const {
  return arg_;
}




}}  // namespace dt::expr
