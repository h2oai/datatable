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
#ifndef dt_EXPR_FEXPR_EXTEND_REMOVE_h
#define dt_EXPR_FEXPR_EXTEND_REMOVE_h
#include "expr/fexpr_func.h"
namespace dt {
namespace expr {


template <bool EXTEND>
class FExpr_Extend_Remove : public FExpr_Func {
  private:
    ptrExpr arg_;
    ptrExpr other_;

  public:
    FExpr_Extend_Remove(ptrExpr&& arg, ptrExpr&& other);
    std::string repr() const override;
    Workframe evaluate_n(EvalContext& ctx) const override;
};

extern template class FExpr_Extend_Remove<true>;
extern template class FExpr_Extend_Remove<false>;


}}  // dt::expr
#endif
