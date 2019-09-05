//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_EXPR_HEAD_h
#define dt_EXPR_HEAD_h
#include "expr/op.h"
#include "expr/declarations.h"
#include "python/obj.h"
namespace dt {
namespace expr {



/**
 * `Head` is the part of an `Expr`. It can be thought as a function
 * without the arguments. For example:
 *
 *    Expr            Head            Arguments
 *    --------------  --------------  ----------
 *    fn(a, b, c)     fn              (a, b, c)
 *    x + y           binary_plus     (x, y)
 *    3               literal_int     ()
 *    [p, q, ..., z]  list            (p, q, ..., z)
 *
 * Each Head object may be parametrized. Generally, the distinction
 * between arguments and parameters is that the arguments in an Expr
 * may only be fully resolved when that Expr is evaluated in a
 * workframe. On the contrary, the parameters are known at the time
 * when the Expr is created.
 *
 * The main API of Head class consists of 3 `evaluate()` methods. Each
 * of them takes a vector of arguments (if any) and the workframe, and
 * produces a list of named `Column`s. The difference between these
 * methods is the context in which the expression is to be evaluated:
 *
 * - evaluate() is the "standard" mode of evaluation;
 *
 * - evaluate_j() computes the expression when it is the root node in
 *     j- or by-expr of DT[i,j,...]. The flag `assignment` is true
 *     when the j-expression is used in an assignment statement
 *     `DT[i,j] = smth`.
 *
 * - evaluate_f() computes the expression when it is used as an
 *     f-selector, i.e. f[expr], or g[expr].
 *
 */
class Head {
  public:
    enum Kind {
      Unknown, None, Bool, Int, Float, Str, Type, Func, List, Frame,
      SliceAll, SliceInt, SliceStr
    };

    virtual ~Head();
    virtual Outputs evaluate(const vecExpr& args, workframe& wf) const = 0;
    virtual Outputs evaluate_j(const vecExpr& args, workframe& wf) const = 0;
    virtual Outputs evaluate_f(workframe& wf, size_t frame_id) const = 0;

    virtual Kind get_expr_kind() const = 0;
};




}}  // namespace dt::expr
#endif
