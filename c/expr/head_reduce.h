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
#ifndef dt_EXPR_HEAD_REDUCE_h
#define dt_EXPR_HEAD_REDUCE_h
#include <string>
#include <unordered_map>
#include <vector>
#include "expr/head_func.h"
namespace dt {
namespace expr {


class Head_Reduce : public Head_Func {
  protected:
    Op op;

  public:
    static void init();  // called once from datatablemodule.h
    explicit Head_Reduce(Op);
    Kind get_expr_kind() const override;
};



//------------------------------------------------------------------------------
// Derived classes
//------------------------------------------------------------------------------

class Head_Reduce_Nullary : public Head_Reduce {
  public:
    using Head_Reduce::Head_Reduce;
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Reduce_Unary : public Head_Reduce {
  public:
    using Head_Reduce::Head_Reduce;
    Outputs evaluate(const vecExpr&, workframe&) const override;
};



class Head_Reduce_Binary : public Head_Reduce {
  public:
    using Head_Reduce::Head_Reduce;
    Outputs evaluate(const vecExpr&, workframe&) const override;
};





}}  // namespace dt::expr
#endif
