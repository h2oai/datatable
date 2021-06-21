//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "column.h"
#include "expr/fexpr_list.h"
#include "expr/fnary/fnary.h"
#include "expr/workframe.h"
#include "stype.h"
#include "utils/exceptions.h"
#include "python/xargs.h"
namespace dt {
namespace expr {


FExpr_RowFn::FExpr_RowFn(ptrExpr&& args)
  : args_(std::move(args))
{}


std::string FExpr_RowFn::repr() const {
  std::string out = name();
  out += "(";
  out += args_->repr();
  out += ")";
  return out;
}


Workframe FExpr_RowFn::evaluate_n(EvalContext& ctx) const {
  Workframe inputs = args_->evaluate_n(ctx);
  Grouping gmode = inputs.get_grouping_mode();
  std::vector<Column> columns;
  columns.reserve(inputs.ncols());
  for (size_t i = 0; i < inputs.ncols(); ++i) {
    columns.emplace_back(inputs.retrieve_column(i));
  }

  Workframe out(ctx);
  out.add_column(
      apply_function(std::move(columns)),
      "", gmode
  );
  return out;
}


SType FExpr_RowFn::common_numeric_stype(const colvec& columns) const {
  SType common_stype = SType::INT32;
  for (size_t i = 0; i < columns.size(); ++i) {
    switch (columns[i].stype()) {
      case SType::BOOL:
      case SType::INT8:
      case SType::INT16:
      case SType::INT32: break;
      case SType::INT64: {
        if (common_stype == SType::INT32) common_stype = SType::INT64;
        break;
      }
      case SType::FLOAT32: {
        if (common_stype == SType::INT32 || common_stype == SType::INT64) {
          common_stype = SType::FLOAT32;
        }
        break;
      }
      case SType::FLOAT64: {
        common_stype = SType::FLOAT64;
        break;
      }
      default:
        throw TypeError() << "Function `" << name() << "` expects a sequence "
                             "of numeric columns, however column " << i
                          << " had type `" << columns[i].stype() << "`";
    }
  }
  #if DT_DEBUG
    if (!columns.empty()) {
      size_t nrows = columns[0].nrows();
      for (const auto& col : columns) xassert(col.nrows() == nrows);
    }
  #endif
  return common_stype;
}


void FExpr_RowFn::promote_columns(colvec& columns, SType target_stype) const {
  for (auto& col : columns) {
    col.cast_inplace(target_stype);
  }
}



/**
  * Python-facing function that implements the n-ary operator.
  *
  * All "rowwise" python functions are implemented using this
  * function, differentiating themselves only with the `args.get_info()`.
  *
  * This function has two possible signatures: it can take either
  * a single Frame argument, in which case the rowwise function will
  * be immediately applied to the frame, and the resulting frame
  * returned; or it can take an Expr or sequence of Exprs as the
  * argument(s), and return a new Expr that encapsulates application
  * of the rowwise function to the given arguments.
  *
  */
py::oobj py_rowfn(const py::XArgs& args) {
  ptrExpr a;
  if (args.num_varargs() == 1) {
    a = as_fexpr(args.vararg(0));
  }
  else {
    a = FExpr_List::empty();
    for (auto arg : args.varargs()) {
      static_cast<FExpr_List*>(a.get())->add_expr(as_fexpr(arg));
    }
  }
  switch (args.get_info()) {
    case FN_ROWALL:   return PyFExpr::make(new FExpr_RowAll(std::move(a)));
    case FN_ROWANY:   return PyFExpr::make(new FExpr_RowAny(std::move(a)));
    case FN_ROWCOUNT: return PyFExpr::make(new FExpr_RowCount(std::move(a)));
    case FN_ROWFIRST: return PyFExpr::make(new FExpr_RowFirstLast<true>(std::move(a)));
    case FN_ROWLAST:  return PyFExpr::make(new FExpr_RowFirstLast<false>(std::move(a)));
    case FN_ROWSUM:   return PyFExpr::make(new FExpr_RowSum(std::move(a)));
    case FN_ROWMAX:   return PyFExpr::make(new FExpr_RowMinMax<false>(std::move(a)));
    case FN_ROWARGMAX:return PyFExpr::make(new FExpr_RowMinMax<false,true>(std::move(a)));
    case FN_ROWMEAN:  return PyFExpr::make(new FExpr_RowMean(std::move(a)));
    case FN_ROWMIN:   return PyFExpr::make(new FExpr_RowMinMax<true>(std::move(a)));
    case FN_ROWARGMIN:return PyFExpr::make(new FExpr_RowMinMax<true,true>(std::move(a)));
    case FN_ROWSD:    return PyFExpr::make(new FExpr_RowSd(std::move(a)));
    default: throw RuntimeError();
  }
}




}}  // namespace dt::expr
