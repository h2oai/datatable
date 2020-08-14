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
#ifndef dt_EXPR_FEXPR_FUNC_h
#define dt_EXPR_FEXPR_FUNC_h
#include "expr/fexpr.h"
namespace dt {
namespace expr {


/**
  * Base class for the largest family of `FExpr`s that are all
  * "function-like". This includes column selectors (eg. `f.A`),
  * functions (eg. `shift(f.A, 1)`), operators (eg. `f.B + 1`),
  * etc.
  *
  * This class is abstract, though it implements many methods from
  * the base "fexpr.h". The derived classes are expected to implement
  * at least the following:
  *
  *   Workframe evaluate_n(EvalContext&) const;
  *   std::string repr() const;
  *
  */
class FExpr_Func : public FExpr {
  public:
    Kind get_expr_kind() const override;

    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;
    int precedence() const noexcept override;

    // Workframe evaluate_n(EvalContext&) const override;
    // std::string repr() const override;
};




}}  // namespace dt::expr
#endif
