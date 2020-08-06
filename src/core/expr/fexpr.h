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
#ifndef dt_EXPR_FEXPR_h
#define dt_EXPR_FEXPR_h
#include "_dt.h"
#include "expr/declarations.h"
namespace dt {
namespace expr {


class FExpr {
  private:
    FExpr* parent_;
    std::vector<std::shared_ptr<FExpr>> children_;

  public:
    FExpr(FExpr* parent, std::vector<std::shared_ptr<FExpr>> children);
    FExpr(FExpr&&) = default;
    FExpr& operator=(FExpr&&) = default;
    virtual ~FExpr() = default;

    virtual Workframe evaluate_n(EvalContext&) = 0;

    /**
      * Return operator precedence of this expression. This will be
      * when generating reprs, so the precendence should roughly
      * correspond to python's:
      *
      *    0   :=
      *    1   lambda
      *    2   if/else
      *    3   or
      *    4   and
      *    5   not
      *    6   in, not in, is, is not, <, <=, >, >=, ==, !=
      *    7   |
      *    8   ^
      *    9   &
      *   10   <<, >>
      *   11   +, - (binary)
      *   12   *, @, /, //, %
      *   13   +, -, ~ (unary)
      *   14   **
      *   15   await
      *   16   x.attr, x[], x()
      *   17   (exprs...), [exprs...], {exprs...}, {key:value...}
      *
      * See also:
      * https://docs.python.org/3/reference/expressions.html#operator-precedence
      */
    virtual int precedence() const noexcept = 0;

    /**
      * Return string representation of this expression, for example
      *
      *   "f.A + 3 * f.B"
      *
      * The implementation must be careful to take the precedence of
      * its arguments into account in order to properly parenthesize
      * them. In the example above, __add__ sees that its lhs argument
      * has precedence 16, and rhs precedence 12 -- thus, the
      * parentheses are not needed, since "+" has lower precedence 11.
      */
    virtual std::string repr() const = 0;
};



}}  // namespace dt::expr
#endif
