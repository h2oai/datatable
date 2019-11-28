//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#ifndef dt_EXPR_EXPR_h
#define dt_EXPR_EXPR_h
#include <memory>
#include <string>
#include <vector>
#include "expr/head.h"
#include "expr/op.h"
#include "expr/declarations.h"
#include "python/obj.h"
#include "column.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Expr
//------------------------------------------------------------------------------

/**
  * `Expr` class is the main building block when evaluating
  * expressions in DT[i,j,...] call. Thus, each i, j and other nodes
  * are represented as `Expr` objects. In turn, each `Expr` contains
  * 0 or more children Exprs called `inputs`. Thus, each i, j and
  * other nodes are in fact trees of Exprs.
  *
  * Each `Expr` node carries a payload `head` pointer. This is an
  * object that contains the actual functionality of the Expr. Please
  * refer to the description of `Head` class for further info.
  *
  * Even though Exprs are used as universal building blocks, their
  * meaning may be different in different contexts. For example,
  * number `3` means a simple 1x1 Frame when used as a function
  * argument, or the 4th column in the Frame when used as `j` node,
  * or the 4th row when used as `i` node, etc. This is why we provide
  * several evaluation modes:
  *
  * evaluate_n()
  *   "Natural" evaluation mode. In this mode the Expr is evaluated
  *   as its most natural meaning, without any special "placement
  *   bonus". Most often this mode is used when Expr is an argument
  *   to some other function. For example, in `3 * <Expr>`, or
  *   `sum(<Expr>)` the <Expr> will be evaluated in natural mode.
  *
  * evaluate_j()
  *   Evaluate Expr as a root j node. In other words, this Expr is
  *   use as `DT[:, <Expr>]` and must be evaluated as such.
  *
  * evaluate_f()
  *   The expression is used as an argument to a FrameProxy symbol:
  *   `f[<Expr>]`. In this mode `frame_id` is also passed to the Expr,
  *   allowing to disambiguate between symbols `f`, `g`, etc.
  *
  * evaluate_i()
  *   The expression is used as the root i node: `DT[<Expr>, :]`.
  *   This function will only be called if there is no `by` node.
  *
  * evaluate_bool()
  *   This is not a "proper" evaluation mode: it is only applied to
  *   boolean expressions (i.e. an expression with `get_expr_kind()`
  *   equal to `Bool`).
  *
  */
class Expr {
  private:
    ptrHead  head;
    vecExpr  inputs;

  public:
    explicit Expr(py::robj src);
    Expr(ptrHead&&, vecExpr&&);
    Expr() = default;
    Expr(Expr&&) = default;
    Expr(const Expr&) = delete;
    Expr& operator=(Expr&&) = default;
    Expr& operator=(const Expr&) = delete;

    Kind get_expr_kind() const;
    operator bool() const noexcept;  // Check whether the Expr is empty or not

    Workframe evaluate_n(EvalContext& ctx, bool allow_new = false) const;
    Workframe evaluate_f(EvalContext& ctx, size_t frame_id, bool allow_new = false) const;
    Workframe evaluate_j(EvalContext& ctx, bool allow_new = false) const;
    Workframe evaluate_r(EvalContext& ctx, const intvec&) const;
    RowIndex  evaluate_i(EvalContext& ctx) const;
    RiGb      evaluate_iby(EvalContext& ctx) const;

    // Evaluate the internal part of the by()/sort() nodes, and return
    // the resulting Workframe, allowing the caller to perform a
    // groupby/sort operation on this Workframe.
    //
    void prepare_by(EvalContext&, Workframe&, std::vector<SortFlag>&) const;

    bool evaluate_bool() const;
    bool is_negated_column(EvalContext&, size_t* iframe, size_t* icol) const;
    int64_t evaluate_int() const;

  private:
    // Construction helpers
    void _init_from_bool(py::robj);
    void _init_from_dictionary(py::robj);
    void _init_from_dtexpr(py::robj);
    void _init_from_ellipsis();
    void _init_from_float(py::robj);
    void _init_from_frame(py::robj);
    void _init_from_int(py::robj);
    void _init_from_iterable(py::robj);
    void _init_from_list(py::robj);
    void _init_from_none();
    void _init_from_numpy(py::robj);
    void _init_from_pandas(py::robj);
    void _init_from_range(py::robj);
    void _init_from_slice(py::robj);
    void _init_from_string(py::robj);
    void _init_from_type(py::robj);
};




}}  // namespace dt::expr
#endif
