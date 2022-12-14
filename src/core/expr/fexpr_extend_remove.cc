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
#include "expr/fexpr_extend_remove.h"
namespace dt {
namespace expr {

template<bool EXTEND>
FExpr_Extend_Remove::FExpr_Extend_Remove(ptrExpr&& arg, ptrExpr&& other) :
  arg_(std::move(arg)),
  other_(std::move(other))
  {}


std::string FExpr_Extend_Remove::repr() const {
  std::string out = EXTEND? "extend" : "remove";
  out += '(';
  out += arg_->repr();
  out += ", arg=";
  out += other_->repr();
  out += ')';
  return out;
}


Workframe FExpr_Extend_Remove::evaluate_n(EvalContext& ctx) const {
  Workframe wf = arg_->evaluate_n(ctx);
  Workframe out = other_->evaluate_n(ctx);
  if (EXTEND){
    wf.cbind(std::move(out));
  } else {
    wf.remove(std::move(out));
  }
  
  return wf;
}


}}  // dt::expr
