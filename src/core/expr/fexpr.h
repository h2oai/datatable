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
  public:
    FExpr() = default;
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


    virtual py::oobj evaluate_pystr() const;
};




//------------------------------------------------------------------------------
// PyFExpr class
//------------------------------------------------------------------------------

/**
  * Python-looking "datatable.FExpr" class. Internally it contains
  * a pointer to the underlying implementation class `FExpr`.
  *
  * The constructor of this class takes a single argument, which can
  * be any object that will get wrapped into an `FExpr`.
  *
  * There is also a no-argument form of the constructor, but it is
  * not intended for public use. Instead, it is used when creating
  * `FExpr`s from C++ via the static `make()` method.
  */
class PyFExpr : public py::XObject<PyFExpr> {
  private:
    ptrExpr expr_;

  public:
    static py::oobj make(FExpr* expr);
    ptrExpr get_expr() const;

    void m__init__(const py::PKArgs&);
    void m__dealloc__();
    py::oobj m__repr__() const;
    py::oobj m__getitem__(py::robj);

    static py::oobj m__compare__  (py::robj, py::robj, int op);
    static py::oobj nb__add__     (py::robj, py::robj);
    static py::oobj nb__sub__     (py::robj, py::robj);
    static py::oobj nb__mul__     (py::robj, py::robj);
    static py::oobj nb__floordiv__(py::robj, py::robj);
    static py::oobj nb__truediv__ (py::robj, py::robj);
    static py::oobj nb__mod__     (py::robj, py::robj);
    static py::oobj nb__pow__     (py::robj, py::robj, py::robj);
    static py::oobj nb__and__     (py::robj, py::robj);
    static py::oobj nb__xor__     (py::robj, py::robj);
    static py::oobj nb__or__      (py::robj, py::robj);
    static py::oobj nb__lshift__  (py::robj, py::robj);
    static py::oobj nb__rshift__  (py::robj, py::robj);
    bool nb__bool__();
    py::oobj nb__invert__();
    py::oobj nb__neg__();
    py::oobj nb__pos__();

    py::oobj len();                        // [DEPRECATED]
    py::oobj re_match(const py::XArgs&);   // [DEPRECATED]

    py::oobj as_type(const py::XArgs&);
    py::oobj count(const py::XArgs&);
    py::oobj countna(const py::XArgs&);
    py::oobj cummax(const py::XArgs&);
    py::oobj cumsum(const py::XArgs&);
    py::oobj extend(const py::XArgs&);
    py::oobj first(const py::XArgs&);
    py::oobj last(const py::XArgs&);
    py::oobj max(const py::XArgs&);
    py::oobj mean(const py::XArgs&);
    py::oobj median(const py::XArgs&);
    py::oobj min(const py::XArgs&);
    py::oobj nunique(const py::XArgs&);
    py::oobj prod(const py::XArgs&);
    py::oobj remove(const py::XArgs&);
    py::oobj rowsum(const py::XArgs&);
    py::oobj rowall(const py::XArgs&);
    py::oobj rowany(const py::XArgs&);
    py::oobj rowcount(const py::XArgs&);
    py::oobj rowfirst(const py::XArgs&);
    py::oobj rowlast(const py::XArgs&);
    py::oobj rowargmax(const py::XArgs&);
    py::oobj rowargmin(const py::XArgs&);
    py::oobj rowmax(const py::XArgs&);
    py::oobj rowmean(const py::XArgs&);
    py::oobj rowmin(const py::XArgs&);
    py::oobj rowsd(const py::XArgs&);
    py::oobj sd(const py::XArgs&);
    py::oobj shift(const py::XArgs&);
    py::oobj sum(const py::XArgs&);

    static void impl_init_type(py::XTypeMaker& xt);
};



}}  // namespace dt::expr
#endif
