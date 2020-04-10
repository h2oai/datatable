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
#ifndef dt_EXPR_HEAD_LIST_h
#define dt_EXPR_HEAD_LIST_h
#include <string>
#include <vector>
#include "expr/head.h"
namespace dt {
namespace expr {


class Head_List : public Head {
  public:
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const sztvec&) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
    void prepare_by(const vecExpr&, EvalContext&, Workframe&, std::vector<SortFlag>&) const override;
};



class Head_NamedList : public Head {
  private:
    strvec names;

  public:
    Head_NamedList(strvec&&);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const sztvec&) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



}}  // namespace dt::expr
#endif
