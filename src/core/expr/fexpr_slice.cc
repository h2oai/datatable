//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "expr/fexpr_slice.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "utils/assert.h"
namespace dt {
namespace expr {


FExpr_Slice::FExpr_Slice(ptrExpr arg, py::robj sliceobj)
  : arg_(std::move(arg)),
    slice_(sliceobj)
{}


Workframe FExpr_Slice::evaluate_n(EvalContext& ctx) const {
  Workframe outputs(ctx);
  return outputs;
}


int FExpr_Slice::precedence() const noexcept {
  return 16;  // Standard python precedence for `x[]` operator. See fexpr.h
}


std::string FExpr_Slice::repr() const {
  // Technically we don't have to put the argument into parentheses if its
  // precedence is equal to 16, however I find that it aids clarity:
  //     (f.A)[:-1]            is better than  f.A[:-1]
  //     (f[0])[::2]           is better than  f[0][::2]
  //     (f.name.lower())[5:]  is better than  f.name.lower()[5:]
  bool parenthesize = (arg_->precedence() <= 16);
  std::string out;
  if (parenthesize) out += "(";
  out += arg_->repr();
  if (parenthesize) out += ")";
  out += "[";
  out += "]";
  return out;
}




}}  // namespace dt::expr
