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
    static ptrHead from_op(Op op, py::otuple params);

    Kind get_expr_kind() const override;
    Outputs evaluate_j(const vecExpr&, workframe&) const override;
    Outputs evaluate_f(workframe&, size_t) const override;
};



//------------------------------------------------------------------------------
// Derived classes
//------------------------------------------------------------------------------

class Head_Func_Column : public Head_Func {
  private:
    size_t frame_id;

  public:
    explicit Head_Func_Column(size_t);
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Func_Cast : public Head_Func {
  private:
    SType stype;
    size_t : 56;

  public:
    explicit Head_Func_Cast(SType);
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Func_Unary : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Unary(Op);
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Func_Binary : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Binary(Op);
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Func_Reduce : public Head_Func {
  private:
    Op op;

  public:
    explicit Head_Func_Reduce(Op);
    Outputs evaluate(const vecExpr&, workframe&) const override;
};





}}  // namespace dt::expr
#endif
