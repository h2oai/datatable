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
#ifndef dt_EXPR_HEAD_LITERAL_h
#define dt_EXPR_HEAD_LITERAL_h
#include <string>
#include <vector>
#include "expr/eval_context.h"
#include "expr/head.h"
#include "python/slice.h"
#include "python/range.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// base Head_Literal
//------------------------------------------------------------------------------

// TODO: remove?
class Head_Literal : public Head {
  public:
    // Workframe evaluate(const vecExpr&, EvalContext&) const override;
    // Workframe evaluate_j(const vecExpr&, EvalContext&) const override;
    // Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;

  protected:
    static Workframe _wrap_column(EvalContext&, Column&&);
};




//------------------------------------------------------------------------------
// implementations
//------------------------------------------------------------------------------

class Head_Literal_None : public Head_Literal {
  public:
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_Bool : public Head_Literal {
  private:
    bool value;
    size_t : 56;

  public:
    explicit Head_Literal_Bool(bool x);
    Kind get_expr_kind() const override;
    bool get_value() const;

    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_Int : public Head_Literal {
  private:
    int64_t value;

  public:
    explicit Head_Literal_Int(int64_t x);
    Kind get_expr_kind() const override;
    int64_t get_value() const;

    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_Float : public Head_Literal {
  private:
    double value;

  public:
    explicit Head_Literal_Float(double x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_String : public Head_Literal {
  private:
    py::oobj pystr;

  public:
    explicit Head_Literal_String(py::robj x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_SliceAll : public Head_Literal {
  public:
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_SliceInt : public Head_Literal {
  private:
    py::oslice value;

  public:
    explicit Head_Literal_SliceInt(py::oslice x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_SliceStr : public Head_Literal {
  private:
    py::oobj start;
    py::oobj end;

  public:
    explicit Head_Literal_SliceStr(py::oslice x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};



class Head_Literal_Range : public Head_Literal {
  private:
    py::orange value;

  public:
    explicit Head_Literal_Range(py::orange x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;

  private:
    std::string _repr_range() const;
};



class Head_Literal_Type : public Head_Literal {
  private:
    py::oobj value;

  public:
    explicit Head_Literal_Type(py::robj x);
    Kind get_expr_kind() const override;
    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};




}}  // namespace dt::expr
#endif
