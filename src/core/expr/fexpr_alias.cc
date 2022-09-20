//------------------------------------------------------------------------------
// Copyright 2022 H2O.ai
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
#include "expr/fexpr_alias.h"
namespace dt {
namespace expr {


FExpr_Alias::FExpr_Alias(ptrExpr&& arg, strvec&& names) :
  arg_(std::move(arg)),
  names_(std::move(names))
  {}


std::string FExpr_Alias::repr() const {
  std::string out = "alias";
  out += '(';
  out += arg_->repr();
  out += ", [";
  for (auto name : names_) {
    out += name;
    out += ",";
  }
  out += "]";
  out += ')';
  return out;
}


Workframe FExpr_Alias::evaluate_n(EvalContext& ctx) const {
  Workframe wf = arg_->evaluate_n(ctx);
  if (wf.ncols() != names_.size()) {
    throw ValueError()
      << "The number of columns does not match the number of names: "
      << wf.ncols() << " vs " << names_.size();
  }

  Workframe wf_out(ctx);
  auto gmode = wf.get_grouping_mode();

  for (size_t i = 0; i < wf.ncols(); ++i) {
    Workframe arg_out(ctx);
    Column col = wf.retrieve_column(i);
    arg_out.add_column(std::move(col), std::string(names_[i]), gmode);
    wf_out.cbind( std::move(arg_out) );
  }

  return wf_out;
}


}}  // dt::expr
