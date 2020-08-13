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
#ifndef dt_EXPR_FEXPR_LITERAL_h
#define dt_EXPR_FEXPR_LITERAL_h
#include "_dt.h"
#include "expr/fexpr.h"
namespace dt {
namespace expr {


class FExpr_Literal_None : public FExpr {
  public:
    explicit FExpr_Literal_None() = default;
    static ptrExpr make();

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    Kind get_expr_kind() const override;
    int precedence() const noexcept override;
    std::string repr() const override;
};



class FExpr_Literal_Bool : public FExpr {
  private:
    bool value_;
    size_t : 56;

  public:
    explicit FExpr_Literal_Bool(bool x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
    bool evaluate_bool() const override;
};



class FExpr_Literal_Int : public FExpr {
  private:
    int64_t value_;

  public:
    explicit FExpr_Literal_Int(int64_t x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
    int64_t evaluate_int() const override;
};



class FExpr_Literal_Float : public FExpr {
  private:
    double value_;

  public:
    explicit FExpr_Literal_Float(double x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
};



class FExpr_Literal_String : public FExpr {
  private:
    py::oobj pystr_;

  public:
    explicit FExpr_Literal_String(py::robj x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
    py::oobj evaluate_pystr() const override;
};



class FExpr_Literal_Slice : public FExpr {
  public:
    static ptrExpr make(py::robj src);
    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    int precedence() const noexcept override;
};



class FExpr_Literal_SliceAll : public FExpr_Literal_Slice {
  public:
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    std::string repr() const override;
    Kind get_expr_kind() const override;
};



class FExpr_Literal_SliceInt : public FExpr_Literal_Slice {
  private:
    py::oslice value_;

  public:
    explicit FExpr_Literal_SliceInt(py::oslice src);

    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    Kind get_expr_kind() const override;
    std::string repr() const override;
};



class FExpr_Literal_SliceStr : public FExpr_Literal_Slice {
  private:
    py::oobj start_;
    py::oobj end_;

  public:
    explicit FExpr_Literal_SliceStr(py::oslice src);

    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    Kind get_expr_kind() const override;
    std::string repr() const override;
};



class FExpr_Literal_Range : public FExpr {
  private:
    py::orange value_;

  public:
    explicit FExpr_Literal_Range(py::orange x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
};



class FExpr_Literal_Type : public FExpr {
  private:
    py::oobj value_;

  public:
    explicit FExpr_Literal_Type(py::robj x);
    static ptrExpr make(py::robj src);

    Workframe evaluate_n(EvalContext&) const override;
    Workframe evaluate_f(EvalContext&, size_t) const override;
    Workframe evaluate_j(EvalContext&) const override;
    Workframe evaluate_r(EvalContext&, const sztvec&) const override;
    RowIndex  evaluate_i(EvalContext&) const override;
    RiGb      evaluate_iby(EvalContext&) const override;

    int precedence() const noexcept override;
    std::string repr() const override;
    Kind get_expr_kind() const override;
};





}}  // namespace dt::expr
#endif
