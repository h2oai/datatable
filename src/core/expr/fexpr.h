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
#include "python/xobject.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// core FExpr class
//------------------------------------------------------------------------------

class FExpr {
  protected:
    FExpr* parent_;
    std::vector<std::shared_ptr<FExpr>> children_;

  public:
    FExpr() : parent_(nullptr) {}
    FExpr(FExpr* parent, std::vector<std::shared_ptr<FExpr>> children);
    FExpr(FExpr&&) = default;
    FExpr& operator=(FExpr&&) = default;
    virtual ~FExpr() = default;

    virtual Workframe evaluate_n(EvalContext&) const = 0;
    virtual Workframe evaluate_f(EvalContext& ctx, size_t frame_id) const = 0;
    virtual Workframe evaluate_j(EvalContext& ctx) const = 0;
    virtual Workframe evaluate_r(EvalContext& ctx, const sztvec&) const = 0;
    virtual RowIndex  evaluate_i(EvalContext&) const = 0;
    virtual RiGb      evaluate_iby(EvalContext&) const = 0;

    // Evaluate the internal part of the by()/sort() nodes, and return
    // the resulting Workframe, allowing the caller to perform a
    // groupby/sort operation on this Workframe.
    //
    virtual void prepare_by(EvalContext&, Workframe&, std::vector<SortFlag>&) const;

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

    /**
      * Categorize this expression according to its "type", enabling
      * special processing for certain kinds of expressions.
      */
    virtual Kind get_expr_kind() const = 0;


    /**
      * If an expression represents unary negation of something, then
      * this method should return the inner expr (without the minus).
      * This is intended for sorting, were `sort(-x)` means "sort by
      * x in descending order".
      */
    virtual std::shared_ptr<FExpr> unnegate_column() const;

    /**
      * If an expression's get_expr_kind() is Kind::Bool, then this
      * method should return this expression converted to a regular
      * boolean value.
      */
    virtual bool evaluate_bool() const;

    /**
      * If an expression's get_expr_kind() is Kind::Int, then this
      * method should return this expression converted to a regular
      * integer.
      */
    virtual int64_t evaluate_int() const;
};



}}  // namespace dt::expr
//------------------------------------------------------------------------------
// Python FExpr class
//------------------------------------------------------------------------------
namespace py {


class FExpr : public XObject<FExpr> {
  private:
    std::shared_ptr<dt::expr::FExpr> expr_;

  public:
    static oobj make(dt::expr::FExpr* expr);
    std::shared_ptr<dt::expr::FExpr> get_expr() const;

    void m__init__(const PKArgs&);
    void m__dealloc__();
    oobj m__repr__() const;

    static oobj m__compare__(robj, robj, int op);
    static oobj nb__add__(robj, robj);
    static oobj nb__sub__(robj, robj);
    static oobj nb__mul__(robj, robj);
    static oobj nb__floordiv__(robj, robj);
    static oobj nb__truediv__(robj, robj);
    static oobj nb__mod__(robj, robj);
    static oobj nb__pow__(robj, robj, robj);
    static oobj nb__and__(robj, robj);
    static oobj nb__xor__(robj, robj);
    static oobj nb__or__(robj, robj);
    static oobj nb__lshift__(robj, robj);
    static oobj nb__rshift__(robj, robj);
    bool nb__bool__();
    oobj nb__invert__();
    oobj nb__neg__();
    oobj nb__pos__();

    oobj extend(const PKArgs&);
    oobj remove(const PKArgs&);
    oobj len();                     // [DEPRECATED]
    oobj re_match(const PKArgs&);   // [DEPRECATED]

    static void impl_init_type(XTypeMaker& xt);
};



}  // namespace py
#endif
