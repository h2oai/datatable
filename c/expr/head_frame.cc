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
#include "expr/workframe.h"
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


Kind Head_Frame::get_expr_kind() const {
  return Kind::Frame;
}



Workframe Head_Frame::evaluate_n(const vecExpr& args, EvalContext& ctx) const {
  (void) args;
  xassert(args.size() == 0);

  if (!(dt->nrows() == ctx.nrows() || dt->nrows() == 1)) {
    throw ValueError() << "Frame has " << dt->nrows() << " rows, and "
        "cannot be used in an expression where " << ctx.nrows()
        << " are expected";
  }
  Grouping grouplevel = (dt->nrows() == 1)? Grouping::GtoONE
                                          : Grouping::GtoALL;
  Workframe res(ctx);
  for (size_t i = 0; i < dt->ncols(); ++i) {
    res.add_column(
        Column(dt->get_column(i)),
        ignore_names? std::string() : std::string(dt->get_names()[i]),
        grouplevel);
  }
  return res;
}


Workframe Head_Frame::evaluate_j(const vecExpr& args, EvalContext& ctx, bool) const {
  return evaluate_n(args, ctx);
}


Workframe Head_Frame::evaluate_f(EvalContext&, size_t, bool) const {
  throw TypeError() << "A Frame cannot be used inside an f-expression";
}


RowIndex Head_Frame::evaluate_i(const vecExpr&, EvalContext& ctx) const {
  if (dt->ncols() != 1) {
    throw ValueError() << "Only a single-column Frame may be used as `i` "
        "selector, instead got a Frame with " << dt->ncols() << " columns";
  }
  SType st = dt->get_column(0).stype();
  if (!(st == SType::BOOL || info(st).ltype() == LType::INT)) {
    throw TypeError() << "A Frame which is used as an `i` selector should be "
        "either boolean or integer, instead got `" << st << "`";
  }

  const Column& col = dt->get_column(0);
  size_t nrows = ctx.nrows();
  if (col.stype() == SType::BOOL) {
    if (col.nrows() != nrows) {
      throw ValueError() << "A boolean column used as `i` selector has "
          << col.nrows() << " row" << (col.nrows() == 1? "" : "s")
          << ", but applied to a Frame with "
           << nrows << " row" << (nrows == 1? "" : "s");
    }
  }
  else if (col.nrows() != 0) {
    int64_t min = col.stats()->min_int();
    int64_t max = col.stats()->max_int();
    if (min < -1) {
      throw ValueError() << "An integer column used as an `i` selector "
          "contains an invalid negative index: " << min;
    }
    if (max >= static_cast<int64_t>(nrows)) {
      throw ValueError() << "An integer column used as an `i` selector "
          "contains index " << max << " which is not valid for a Frame with "
          << nrows << " row" << (nrows == 1? "" : "s");
    }
  }

  return RowIndex(col);
}


RiGb Head_Frame::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A Frame cannot be used as an i-selector "
                       "in the presence of a groupby";
}




}}  // namespace dt::expr
