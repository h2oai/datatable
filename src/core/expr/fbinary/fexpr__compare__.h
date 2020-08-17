//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifndef dt_EXPR_FBINARY_FEXPR__COMPARE__h
#define dt_EXPR_FBINARY_FEXPR__COMPARE__h
#include "expr/fbinary/fexpr_binaryop.h"
namespace dt {
namespace expr {



class FExpr__eq__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};



class FExpr__ne__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};



class FExpr__lt__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};



class FExpr__gt__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};



class FExpr__le__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};



class FExpr__ge__ : public FExpr_BinaryOp {
  public:
    using FExpr_BinaryOp::FExpr_BinaryOp;

    std::string name() const override;
    int precedence() const noexcept override;
    Column evaluate1(Column&& lcol, Column&& rcol) const override;
};





}}  // namespace dt::expr
#endif
