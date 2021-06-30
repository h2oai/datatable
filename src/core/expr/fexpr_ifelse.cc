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
#include "column/ifelse.h"        // IfElse_ColumnImpl
#include "column/ifelsen.h"       // IfElseN_ColumnImpl
#include "documentation.h"
#include "expr/eval_context.h"    // EvalContext
#include "expr/fexpr_func.h"      // FExpr_Func
#include "expr/workframe.h"       // Workframe
#include "python/xargs.h"         // DECLARE_PYFN
#include "stype.h"                // SType
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// FExpr_IfElse
//------------------------------------------------------------------------------

class FExpr_IfElse : public FExpr_Func {
  private:
    // The number of conditions is 1 less than the number of values
    vecExpr conditions_;
    vecExpr values_;

  public:
    FExpr_IfElse(vecExpr&&, vecExpr&&);
    Workframe evaluate_n(EvalContext&) const override;
    std::string repr() const override;
};



FExpr_IfElse::FExpr_IfElse(vecExpr&& conditions, vecExpr&& values)
  : conditions_(std::move(conditions)),
    values_(std::move(values))
{
  xassert(conditions_.size() == values_.size() - 1);
}


Workframe FExpr_IfElse::evaluate_n(EvalContext& ctx) const {
  size_t n = conditions_.size();
  xassert(n >= 1);

  std::vector<Workframe> all_workframes;
  for (const auto& cond : conditions_) {
    all_workframes.push_back(cond->evaluate_n(ctx));
  }
  for (const auto& value : values_) {
    all_workframes.push_back(value->evaluate_n(ctx));
  }
  xassert(all_workframes.size() == 2*n + 1);
  for (size_t j = 0; j < all_workframes.size(); ++j) {
    if (all_workframes[j].ncols() != 1) {
      auto jj = (j < n)? j : j - n;
      throw TypeError() << "The `" << (j==jj? "condition" : "value")
          << (jj+1) << "` argument in ifelse() cannot be a multi-column "
                       "expression";
    }
  }
  auto gmode = Workframe::sync_grouping_mode(all_workframes);

  colvec condition_cols;
  for (size_t j = 0; j < n; ++j) {
    Column col = all_workframes[j].retrieve_column(0);
    if (col.stype() != SType::BOOL) {
      throw TypeError()
          << "The `condition" << (j+1)
          << "` argument in ifelse() must be a boolean column";
    }
    condition_cols.push_back(std::move(col));
  }

  colvec value_cols;
  SType out_stype = SType::VOID;
  for (size_t j = n; j < all_workframes.size(); ++j) {
    Column col = all_workframes[j].retrieve_column(0);
    out_stype = common_stype(out_stype, col.stype());
    value_cols.push_back(std::move(col));
  }
  for (Column& col : value_cols) {
    col.cast_inplace(out_stype);
  }

  Workframe wf_out(ctx);
  if (n == 1) {
    wf_out.add_column(Column{
      new IfElse_ColumnImpl(
          std::move(condition_cols[0]),
          std::move(value_cols[0]),
          std::move(value_cols[1]))
      }, std::string(), gmode
    );
  } else {
    wf_out.add_column(
      Column{
        new IfElseN_ColumnImpl(std::move(condition_cols), std::move(value_cols))
      }, std::string(), gmode
    );
  }
  return wf_out;
}


std::string FExpr_IfElse::repr() const {
  std::string out = "ifelse(";
  for (size_t i = 0; i < conditions_.size(); ++i) {
    out += conditions_[i]->repr();
    out += ", ";
    out += values_[i]->repr();
    out += ", ";
  }
  out += values_.back()->repr();
  out += ')';
  return out;
}




//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------

static py::oobj ifelse(const py::XArgs& args) {
  auto n = args.num_varargs();
  if (n < 3) {
    throw TypeError()
      << "Function `datatable.ifelse()` requires at least 3 arguments";
  }
  if (n % 2 != 1) {
    throw TypeError()
      << "Missing the required `default` argument in function "
         "`datatable.ifelse()`";
  }
  vecExpr conditions;
  vecExpr values;
  for (size_t i = 0; i < n/2; i++) {
    conditions.push_back(as_fexpr(args.vararg(i*2)));
    values.push_back(as_fexpr(args.vararg(i*2 + 1)));
  }
  values.push_back(as_fexpr(args.vararg(n - 1)));
  return dt::expr::PyFExpr::make(
              new dt::expr::FExpr_IfElse(std::move(conditions),
                                         std::move(values))
         );
}

DECLARE_PYFN(&ifelse)
    ->name("ifelse")
    ->docs(doc_dt_ifelse)
    ->allow_varargs();




}}  // dt::expr
