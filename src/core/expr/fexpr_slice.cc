//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "column/string_slice.h"
#include "expr/fexpr_column.h"
#include "expr/fexpr_slice.h"
#include "expr/eval_context.h"
#include "expr/workframe.h"
#include "utils/assert.h"
namespace dt {
namespace expr {



FExpr_Slice::FExpr_Slice(ptrExpr arg, py::robj start, py::robj stop, py::robj step)
  : arg_(std::move(arg)),
    start_(as_fexpr(start)),
    stop_(as_fexpr(stop)),
    step_(as_fexpr(step))
{}


int FExpr_Slice::precedence() const noexcept {
  return 16;  // Standard python precedence for `x[]` operator. See fexpr.h
}


std::string FExpr_Slice::repr() const {
  // Technically we don't have to put the argument into parentheses if its
  // precedence is equal to 16, however I find that it aids clarity:
  //     (f.A)[:-1]            is better than  f.A[:-1]
  //     (f[0])[::2]           is better than  f[0][::2]
  //     (f.name.lower())[5:]  is better than  f.name.lower()[5:]
  bool parenthesize = (arg_->precedence() <= 16);
  std::string out;
  if (parenthesize) out += "(";
  out += arg_->repr();
  if (parenthesize) out += ")";
  out += "[";
  // `:` operator in a slice has precedence around 0: it's smaller than
  // the precedence of lambda, but larger than precedence of `,`.
  bool hasStart = (start_->get_expr_kind() != Kind::None);
  bool hasStop = (stop_->get_expr_kind() != Kind::None);
  bool hasStep = (step_->get_expr_kind() != Kind::None);
  bool addSpaces = (hasStart && start_->get_expr_kind() != Kind::Int) ||
                   (hasStop && stop_->get_expr_kind() != Kind::Int) ||
                   (hasStep && step_->get_expr_kind() != Kind::Int);
  if (hasStart) {
    out += start_->repr();
    if (addSpaces) out += " ";
  }
  out += ":";
  if (hasStop) {
    if (addSpaces) out += " ";
    out += stop_->repr();
  }
  if (hasStep) {
    out += addSpaces? " : " : ":";
    out += step_->repr();
  }
  out += "]";
  return out;
}



Workframe FExpr_Slice::evaluate_n(EvalContext& ctx) const {
  std::vector<Workframe> wfs;
  wfs.push_back(arg_->evaluate_n(ctx));
  wfs.push_back(start_->evaluate_n(ctx));
  wfs.push_back(stop_->evaluate_n(ctx));
  wfs.push_back(step_->evaluate_n(ctx));
  if (wfs[0].ncols() != 1) {
    throw TypeError() << "Slice cannot be applied to multi-column expressions";
  }
  if (wfs[1].ncols() != 1 || wfs[2].ncols() != 1 || wfs[3].ncols() != 1) {
    throw TypeError() << "Cannot use multi-column expressions inside a slice";
  }
  auto gmode = Workframe::sync_grouping_mode(wfs);

  Workframe result(ctx);
  auto argCol = wfs[0].retrieve_column(0);
  if (!argCol.type().is_string()) {
    throw TypeError()
        << "Slice expression can only be applied to a column of string type";
  }
  auto startCol = wfs[1].retrieve_column(0);
  auto stopCol = wfs[2].retrieve_column(0);
  auto stepCol = wfs[3].retrieve_column(0);
  if (!startCol.type().is_integer() || !stopCol.type().is_integer() ||
      !stepCol.type().is_integer()) {
    throw TypeError()
        << "Non-integer expressions cannot be used inside a slice";
  }
  startCol.cast_inplace(Type::int64());
  stopCol.cast_inplace(Type::int64());
  stepCol.cast_inplace(Type::int64());
  result.add_column(
      Column(new StringSlice_ColumnImpl(
                std::move(argCol),
                std::move(startCol),
                std::move(stopCol),
                std::move(stepCol)
              )),
      wfs[0].retrieve_name(0),
      gmode
  );
  return result;
}




}}  // namespace dt::expr
