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
#ifndef dt_EXPR_HEAD_FUNC_h
#define dt_EXPR_HEAD_FUNC_h
#include <string>
#include <unordered_map>
#include <vector>
#include "expr/head.h"
#include "python/tuple.h"
namespace dt {
namespace expr {


class Head_Func : public Head {
  private:
    using maker_t = ptrHead(*)(Op, const py::otuple&);
    static std::unordered_map<size_t, maker_t> factory;

  public:
    static void init();  // called once from datatablemodule.h
    static ptrHead from_op(Op op, const py::otuple& params);

    Kind get_expr_kind() const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;

    Workframe evaluate_r(const vecExpr& args,
                         EvalContext& ctx,
                         const intvec& column_indices) const override;
};



//------------------------------------------------------------------------------
// Derived classes
//------------------------------------------------------------------------------

class Head_Func_Column : public Head_Func {
  private:
    size_t frame_id;

  public:
    explicit Head_Func_Column(size_t);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_Cast : public Head_Func {
  private:
    SType stype;
    size_t : 56;

  public:
    explicit Head_Func_Cast(SType);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_Colset : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Colset(Op);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_Unary : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Unary(Op);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;

    Op get_op() const { return op; }
};



class Head_Func_Binary : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Binary(Op);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_Nary : public Head_Func {
  private:
    Op op_;

  public:
    explicit Head_Func_Nary(Op);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_Shift : public Head_Func {
  private:
    int shift_;
    int : 32;

  public:
    static ptrHead make(Op, const py::otuple& params);

    Head_Func_Shift(int shift);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};



class Head_Func_IsClose : public Head_Func {
  private:
    double rtol_;
    double atol_;

  public:
    static ptrHead make(Op, const py::otuple& params);

    Head_Func_IsClose(double rtol, double atol);
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
};




}}  // namespace dt::expr
#endif
