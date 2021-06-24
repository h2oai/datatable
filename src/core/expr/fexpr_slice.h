//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_EXPR_FEXPR_SLICE_h
#define dt_EXPR_FEXPR_SLICE_h
#include "expr/fexpr_func.h"
#include "python/obj.h"
namespace dt {
namespace expr {



/**
  * Class for expressions such as `f.A[1:]`. This is only relevant
  * for string-type columns; and in the future for list-types
  * and maybe some others too.
  *
  * Also note that we want to support the case when the slice
  * expression itself is an expression. For example, a formula like
  * this to remove everything after and including the '#' symbol is
  * perfectly common:
  *
  *     (f.A)[f.A.index('#') + 1:]
  *
  */
class FExpr_Slice : public FExpr_Func {
  private:
    ptrExpr arg_;
    ptrExpr start_;
    ptrExpr stop_;
    ptrExpr step_;


  public:
    FExpr_Slice(ptrExpr arg, py::robj start, py::robj stop, py::robj step);

    int precedence() const noexcept override;
    std::string repr() const override;
    Workframe evaluate_n(EvalContext&) const override;
};




}}  // namespace dt::expr
#endif
