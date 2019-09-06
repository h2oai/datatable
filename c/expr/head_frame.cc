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
#include "expr/expr.h"
#include "expr/head_frame.h"
#include "expr/outputs.h"
#include "frame/py_frame.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


ptrHead Head_Frame::from_datatable(py::robj src) {
  return ptrHead(new Head_Frame(src));
}

ptrHead Head_Frame::from_numpy(py::robj src) {
  py::oobj src_frame = py::Frame::oframe(src);
  return ptrHead(new Head_Frame(src_frame, /* ignore_names= */ true));
}

ptrHead Head_Frame::from_pandas(py::robj src) {
  py::oobj src_frame = py::Frame::oframe(src);
  return ptrHead(new Head_Frame(src_frame));
}


Head_Frame::Head_Frame(py::robj src, bool ignore_names_)
  : container(src),
    dt(src.to_datatable()),
    ignore_names(ignore_names_) {}


Head::Kind Head_Frame::get_expr_kind() const {
  return Head::Kind::Frame;
}



Outputs Head_Frame::evaluate(const vecExpr& args, workframe& wf) const {
  (void) args;
  xassert(args.size() == 0);

  if (!(dt->nrows == wf.nrows() || dt->nrows == 1)) {
    throw ValueError() << "Frame has " << dt->nrows << " rows, and "
        "cannot be used in an expression where " << wf.nrows()
        << " are expected";
  }
  Grouping grouplevel = (dt->nrows == 1)? Grouping::GtoONE
                                        : Grouping::GtoALL;
  Outputs res;
  for (size_t i = 0; i < dt->ncols; ++i) {
    res.add(Column(dt->get_column(i)),
            ignore_names? std::string() : std::string(dt->get_names()[i]),
            grouplevel);
  }
  return res;
}


Outputs Head_Frame::evaluate_j(const vecExpr& args, workframe& wf) const {
  return evaluate(args, wf);
}


Outputs Head_Frame::evaluate_f(workframe&, size_t) const {
  throw TypeError() << "A Frame cannot be used inside an f-expression";
}


}}  // namespace dt::expr
