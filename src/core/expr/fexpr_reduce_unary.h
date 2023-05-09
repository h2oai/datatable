//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#ifndef dt_EXPR_FEXPR_REDUCE_UNARY_h
#define dt_EXPR_FEXPR_REDUCE_UNARY_h
#include "expr/fexpr_func.h"
namespace dt {
namespace expr {


/**
  * Base class for FExpr reducers that have only one parameter.
  */
class FExpr_ReduceUnary : public FExpr_Func {
  protected:
    ptrExpr arg_;

  public:
    FExpr_ReduceUnary(ptrExpr&&);
    Workframe evaluate_n(EvalContext&) const override;
    std::string repr() const override;

    // API for the derived classes
    virtual Column evaluate1(Column&& col, const Groupby& gby, bool is_grouped) const = 0;
    virtual std::string name() const = 0;
};



}}  // namespace dt::expr
#endif
