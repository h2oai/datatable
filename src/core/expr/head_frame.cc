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
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_frame.h"
#include "expr/workframe.h"
#include "frame/py_frame.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

ptrHead Head_Frame::from_datatable(py::robj src) {
  return ptrHead(new Head_Frame(src));
}

ptrHead Head_Frame::from_numpy(py::robj src) {
  py::oobj src_frame = py::Frame::oframe(src);
  return ptrHead(new Head_Frame(src_frame, /* ignore_names_= */ true));
}

ptrHead Head_Frame::from_pandas(py::robj src) {
  py::oobj src_frame = py::Frame::oframe(src);
  return ptrHead(new Head_Frame(src_frame));
}


Head_Frame::Head_Frame(py::robj src, bool ignore_names)
  : container_(src),
    dt_(src.to_datatable()),
    ignore_names_(ignore_names) {}


Kind Head_Frame::get_expr_kind() const {
  return Kind::Frame;
}



//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

// If X is a Frame (or a numpy array), then an expression such as
//
//   DT[:, f.A + X]
//
// is perfectly reasonable: column A in DT should be added element-
// wise to the column(s) in frame X. This is allowed provided that
// X and DT have the same number of rows (or if X has a single row).
//
// Thus, X in this case is trivially joined to DT row-by-row. For
// more advanced types of joins, an explicit `join()` clause has to
// be used.
//
Workframe Head_Frame::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  (void) args;
  xassert(args.size() == 0);

  size_t nrows = dt_->nrows();
  if (!(nrows == ctx.nrows() || nrows == 1)) {
    throw ValueError() << "Frame has " << nrows << " rows, and "
        "cannot be used in an expression where " << ctx.nrows()
        << " are expected";
  }
  Grouping grouplevel = (nrows == 1)? Grouping::SCALAR
                                    : Grouping::GtoALL;
  Workframe res(ctx);
  for (size_t i = 0; i < dt_->ncols(); ++i) {
    res.add_column(
        Column(dt_->get_column(i)),
        ignore_names_? std::string() : std::string(dt_->get_names()[i]),
        grouplevel);
  }
  return res;
}


// If X is a Frame, then using it as j-node in DT[i,j] is essentially
// the following: DT[:, X], and it means the same as simply X. This is
// done for consistency with "normal" evaluation cases.
//
// In addition, standalone X in j can be used to with an i-filter:
// DT[<i>, X] is thus equivalent to X[DT[:, <i>], :].
//
Workframe Head_Frame::evaluate_j(
    const vecExpr& args, EvalContext& ctx, bool allow_new) const
{
  return evaluate_n(args, ctx, allow_new);
}


// If X is a Frame, and it is used in the expression
//
//   DT[:, j] = X
//
// Then the columns of X are used as-is, i.e. use "normal" evaluation
// mode. The stypes of the RHS can be ignored, since the stypes of X
// take precedence in this case.
//
Workframe Head_Frame::evaluate_r(
    const vecExpr& args, EvalContext& ctx, const intvec& indices) const
{
  // Allow to assign an empty frame to an empty column set (see issue #1544)
  if (indices.size() == 0 && dt_->nrows() == 0 && dt_->ncols() == 0) {
    return Workframe(ctx);
  }
  return evaluate_n(args, ctx, false);
}



// If X is a Frame, then the expression f[X] (as in DT[:, f[X]]) just
// doesn't make much sense, so we disallow it.
//
Workframe Head_Frame::evaluate_f(EvalContext&, size_t, bool) const {
  throw TypeError() << "A Frame cannot be used inside an f-expression";
}


// When a Frame X is used as an i-node in DT[X, :], then the following
// two cases are allowed:
//
//   - X is a single boolean column with the same number of rows as
//     DT: in this case X serves as a filter on DT's rows;
//
//   - X is a single integer column, where the integer values do not
//     exceed DT.nrows: in this case X serves as a rowindex, i.e. it
//     indicates which rows from DT ought to be taken.
//
// Note that we do not allow notation DT[X, :] to indicate a join (as
// R data.table does): such use is too confusing, and violates the
// basic API convention that i node is used to indicate row selection
// only.
//
// We may in the future add a third case where X is a single-column
// *keyed* frame, in which case an implicit join on the same-named
// column in DT could be performed. That is, DT[X, :] could be taken
// to mean the same as DT[X == f[X.name], :]. For now, however, the
// use of keyed frames in i node is disallowed.
//
RowIndex Head_Frame::evaluate_i(const vecExpr&, EvalContext& ctx) const {
  if (dt_->ncols() != 1) {
    throw ValueError() << "Only a single-column Frame may be used as `i` "
        "selector, instead got a Frame with " << dt_->ncols() << " columns";
  }
  if (dt_->nkeys()) {
    throw NotImplError() << "A keyed frame cannot be used as an i selector";
  }
  const Column& col = dt_->get_column(0);
  SType st = col.stype();
  if (!(st == SType::BOOL || info(st).ltype() == LType::INT)) {
    throw TypeError() << "A Frame which is used as an `i` selector should be "
        "either boolean or integer, instead got `" << st << "`";
  }

  size_t nrows = ctx.nrows();
  if (st == SType::BOOL) {
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
    if (min < 0) {
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


// When X is a single-column Frame, then using it as an i-node in the
// presence of a groupby condition is disallowed:
//
//   DT[X, :, by(f.id)]  # error
//
// I cannot think of a good interpretation of such notation.
//
RiGb Head_Frame::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw TypeError() << "A Frame cannot be used as an i-selector "
                       "in the presence of a groupby";
}




}}  // namespace dt::expr
