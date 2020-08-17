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
#include "expr/fbinary/fexpr__compare__.h"
#include "expr/op.h"  // tmp
namespace dt {
namespace expr {


// tmp
static py::oobj make_binexpr(Op op, py::robj lhs, py::robj rhs) {
  return py::robj(py::Expr_Type).call({
                  py::oint(static_cast<int>(op)),
                  py::otuple{lhs, rhs}});
}


py::oobj PyFExpr::m__compare__(py::robj x, py::robj y, int op) {
  switch (op) {
    case Py_EQ: return PyFExpr::make(new FExpr__eq__(as_fexpr(x), as_fexpr(y)));
    case Py_NE: return make_binexpr(dt::expr::Op::NE, x, y);
    case Py_LT: return make_binexpr(dt::expr::Op::LT, x, y);
    case Py_LE: return make_binexpr(dt::expr::Op::LE, x, y);
    case Py_GT: return make_binexpr(dt::expr::Op::GT, x, y);
    case Py_GE: return make_binexpr(dt::expr::Op::GE, x, y);
    default:
      throw RuntimeError() << "Unknown op " << op << " in __compare__";  // LCOV_EXCL_LINE
  }
}



}}  // namespace dt::expr
