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
  * `Head` is the part of an `Expr`. It can be thought of as a
  * function without the arguments. For example:
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
  * EvalContext, whereas the parameters are known at the time when
  * the Expr is created.
  *
  * The main API of Head class consists of several `evaluate()`
  * methods. Each of them takes a vector of arguments (if any) and the
  * `EvalContext`, and produces the result which is appropriate for
  * that particular evaluation mode. All of these methods must be
  * implemented by subclasses:
  *
  * - evaluate_n() is the "standard" mode of evaluation, producing
  *     a `Workframe`;
  *
  * - evaluate_j() computes the expression when it is the root node in
  *     j- or by-expr of DT[i,j,...]. The flag `assignment` is true
  *     when the j-expression is used in an assignment statement
  *     `DT[i,j] = smth`.
  *
  * - evaluate_r() computes the expression when it is used as a
  *     replacement value, i.e. compute R in `DT[i,j] = R`.
  *
  * - evaluate_f() computes the expression when it is used as an
  *     f-selector, i.e. f[expr], or g[expr].
  *
  * - evaluate_i() computes the expression when it is used as the
  *     i-node in DT[i,j,...].
  *
  * - evaluate_iby()
  *
  *
  * The hierarchy of Head subclasses is the following:
  *
  *   Head
  *     +-- Head_Frame
  *     +-- Head_Func
  *     |     +-- Head_Func_Binary
  *     |     +-- Head_Func_Cast
  *     |     +-- Head_Func_Colset
  *     |     +-- Head_Func_Column
  *     |     +-- Head_Func_Unary
  *     |     +-- Head_Reduce
  *     |           +-- Head_Reduce_Binary
  *     |           +-- Head_Reduce_Nullary
  *     |           +-- Head_Reduce_Unary
  *     +-- Head_List
  *     +-- Head_Literal
  *     |     +-- Head_Literal_Bool
  *     |     +-- Head_Literal_Float
  *     |     +-- Head_Literal_Int
  *     |     +-- Head_Literal_None
  *     |     +-- Head_Literal_Range
  *     |     +-- Head_Literal_SliceAll
  *     |     +-- Head_Literal_SliceInt
  *     |     +-- Head_Literal_SliceStr
  *     |     +-- Head_Literal_String
  *     |     +-- Head_Literal_Type
  *     +-- Head_NamedList
  *
  */
class Head {
  public:
    virtual ~Head();

    virtual Workframe evaluate_n(const vecExpr& args,
                                 EvalContext& ctx,
                                 bool allow_new) const = 0;

    virtual Workframe evaluate_j(const vecExpr& args,
                                 EvalContext& ctx,
                                 bool allow_new) const = 0;

    virtual Workframe evaluate_r(const vecExpr& args,
                                 EvalContext& ctx,
                                 const intvec& column_indices) const = 0;

    virtual Workframe evaluate_f(EvalContext& ctx,
                                 size_t frame_id,
                                 bool allow_new) const = 0;

    virtual RowIndex evaluate_i(const vecExpr& args,
                                EvalContext& ctx) const = 0;

    // Evaluate the expression when it is used as the by() node. The
    // result of the evaluation is a Workframe that will be later used
    // in the group() operation.
    //
    virtual void prepare_by(const vecExpr& args, EvalContext& ctx,
                            Workframe& wf, std::vector<SortFlag>& flags) const;

    virtual RiGb evaluate_iby(const vecExpr& args,
                              EvalContext& ctx) const = 0;

    virtual Kind get_expr_kind() const = 0;
};




}}  // namespace dt::expr
#endif
