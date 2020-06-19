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
#include "expr/eval_context.h"    // EvalContext
#include "expr/expr.h"            // Expr
#include "expr/head_func.h"       // Head_Func_IfElse
#include "expr/workframe.h"       // Workframe
#include "datatablemodule.h"      // DatatableModule
#include "stype.h"                // SType



//------------------------------------------------------------------------------
// Head_Func_IfElse
//------------------------------------------------------------------------------
namespace dt {
namespace expr {


Workframe Head_Func_IfElse::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 3);
  Workframe wf_cond = args[0].evaluate_n(ctx);
  Workframe wf_true = args[1].evaluate_n(ctx);
  Workframe wf_false = args[2].evaluate_n(ctx);
  if (wf_cond.ncols() != 1 || wf_true.ncols() != 1 || wf_false.ncols() != 1) {
    throw TypeError() << "Multi-column expressions are not supported in "
        "`ifelse()` function";
  }
  wf_cond.sync_grouping_mode(wf_true);
  wf_cond.sync_grouping_mode(wf_false);
  wf_true.sync_grouping_mode(wf_false);
  auto gmode = wf_cond.get_grouping_mode();

  Column col_cond = wf_cond.retrieve_column(0);
  Column col_true = wf_true.retrieve_column(0);
  Column col_false= wf_false.retrieve_column(0);

  if (col_cond.stype() != SType::BOOL) {
    throw TypeError()
        << "The `condition` argument in ifelse() must be a boolean column";
  }
  SType out_stype = common_stype(col_true.stype(), col_false.stype());
  col_true.cast_inplace(out_stype);
  col_false.cast_inplace(out_stype);
  Column out_col {new IfElse_ColumnImpl(
      std::move(col_cond),
      std::move(col_true),
      std::move(col_false))
  };

  Workframe wf_out(ctx);
  wf_out.add_column(std::move(out_col), std::string(), gmode);
  return wf_out;
}



ptrHead Head_Func_IfElse::make(Op, const py::otuple& params) {
  xassert(params.size() == 0); (void) params;
  return ptrHead(new Head_Func_IfElse());
}



}}  // dt::expr


//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------
namespace py {


static const char* doc_ifelse =
R"(ifelse(condition, expr_if_true, expr_if_false)
--

Produce a column that chooses one of the two values based on the
condition.

This function will only compute those values that are needed for
the result. Thus, for each row we will evaluate either `expr_if_true`
or `expr_if_false` (based on the `condition` value) but not both.
This may be relevant for those cases

Parameters
----------
condition: Expr
    An expression yielding a single boolean column.

expr_if_true: Expr
    Values that will be used when the condition evaluates to True.
    This must be a single column (or equivalent).

expr_if_false: Expr
    Values that will be used when the condition evaluates to False.
    This must be a single column (or equivalent).

(return): Expr
    The produced expression, is a single column whose stype is the
    stype which is common for `expr_if_true` and `expr_if_false`,
    i.e. it is the smallest stype into which both exprs can be
    upcasted.
)";

static PKArgs args_ifelse(
    3, 0, 0, false, false,
    {"condition", "expr_if_true", "expr_if_false"},
    "ifelse", doc_ifelse);

static oobj ifelse(const PKArgs& args)
{
  robj arg_cond = args[0].to_robj();
  robj arg_true = args[1].to_robj();
  robj arg_false = args[2].to_robj();
  if (!arg_cond || !arg_true || !arg_false) {
    throw TypeError() << "Function `ifelse()` requires 3 arguments";
  }
  return robj(Expr_Type).call({
      oint(static_cast<size_t>(dt::expr::Op::IFELSE)),
      otuple({arg_cond, arg_true, arg_false})
  });
}



void DatatableModule::init_methods_ifelse() {
  ADD_FN(&ifelse, args_ifelse);
}



}
