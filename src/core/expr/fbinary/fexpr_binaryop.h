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
#ifndef dt_EXPR_FEXPR_BINARYOP_h
#define dt_EXPR_FEXPR_BINARYOP_h
#include "expr/fexpr_func.h"
namespace dt {
namespace expr {


/**
  * Base class for binary operators / functions.
  *
  * The implementation of `evaluate_n` handles multi-column lhs & rhs
  * while relying on the child's implementing the `evaluate1()`
  * method that deals only with the single-column case.
  *
  * The implementation of `repr()` produces the standard `lhs OP rhs`
  * form, taking into account precedences of lhs and rhs in order to
  * apply the parentheses correctly. The child needs only to implement
  * the `name()` method that provides stringification of its OP.
  */
class FExpr_BinaryOp : public FExpr_Func {
  protected:
    ptrExpr lhs_;
    ptrExpr rhs_;

  public:
    FExpr_BinaryOp(ptrExpr&& x, ptrExpr&& y);

    Workframe evaluate_n(EvalContext&) const override;
    std::string repr() const override;

    virtual Column evaluate1(Column&& lcol, Column&& rcol) const = 0;
    virtual std::string name() const = 0;
};




}}  // namespace dt::expr
#endif
