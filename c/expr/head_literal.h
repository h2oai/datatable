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
#include "expr/head.h"
namespace dt {
namespace expr {


class Head_Literal : public Head {
  public:
    static ptrHead from_none();
    static ptrHead from_bool(bool);
    static ptrHead from_int(int64_t);
    static ptrHead from_float(double);
    static ptrHead from_string(py::robj);

    Outputs evaluate(const vecExpr&, workframe&) const override;
    Outputs evaluate_j(const vecExpr&, workframe&) const override;
    Outputs evaluate_f(const vecExpr&, workframe&, size_t) const override;

    virtual Column eval_as_literal() const = 0;
    virtual Outputs eval_as_selector(workframe&, size_t frame_id) const = 0;
};



}}  // namespace dt::expr
#endif
