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
#ifndef dt_EXPR_FEXPR_COLUMN_h
#define dt_EXPR_FEXPR_COLUMN_h
#include "expr/fexpr_func.h"
#include "python/obj.h"
namespace dt {
namespace expr {


/**
  * Class for expressions such as `f.A`.
  */
class FExpr_ColumnAsAttr : public FExpr_Func {
  private:
    size_t namespace_;
    py::oobj pyname_;

  public:
    FExpr_ColumnAsAttr(size_t ns, py::robj name);

    Workframe evaluate_n(EvalContext&) const override;
    int precedence() const noexcept override;
    std::string repr() const override;

    py::oobj get_pyname() const;
};



/**
  * Class for expressions such as `f[x]`.
  */
class FExpr_ColumnAsArg : public FExpr_Func {
  private:
    size_t namespace_;
    ptrExpr arg_;

  public:
    FExpr_ColumnAsArg(size_t ns, py::robj arg);

    Workframe evaluate_n(EvalContext&) const override;
    int precedence() const noexcept override;
    std::string repr() const override;

    ptrExpr get_arg() const;
};




}}  // namespace dt::expr
#endif
