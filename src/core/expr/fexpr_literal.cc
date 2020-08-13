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
#include "expr/fexpr_literal.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
namespace dt {
namespace expr {


ptrExpr FExpr_Literal_Slice::make(py::robj src) {
  auto ss = src.to_oslice();
  if (ss.is_trivial()) {
    return ptrExpr(new FExpr_Literal_SliceAll);
  }
  else if (ss.is_numeric()) {
    return ptrExpr(new FExpr_Literal_SliceInt(ss));
  }
  else if (ss.is_string()) {
    return ptrExpr(new FExpr_Literal_SliceStr(ss));
  }
  else {
    throw TypeError() << src << " is neither integer- nor string- valued";
  }
}



Workframe FExpr_Literal_Slice::evaluate_n(EvalContext&) const {
  throw TypeError() << "A slice expression cannot appear in this context";
}


Workframe FExpr_Literal_Slice::evaluate_r(EvalContext&, const sztvec&) const {
  throw TypeError() << "A slice expression cannot appear in this context";
}


int FExpr_Literal_Slice::precedence() const noexcept {
  return 0;
}





}}  // namespace dt::expr
