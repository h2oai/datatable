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
#include "column.h"
#include "column/isclose.h"
#include "datatablemodule.h"
#include "documentation.h"
#include "ltype.h"
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "python/tuple.h"
#include "python/xargs.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Head_Func_IsClose
//------------------------------------------------------------------------------

Head_Func_IsClose::Head_Func_IsClose(double rtol, double atol)
  : rtol_(rtol), atol_(atol)
{
  xassert(rtol >= 0 && atol >= 0);
}


ptrHead Head_Func_IsClose::make(Op, const py::otuple& params) {
  xassert(params.size() == 2);
  double rtol = params[0].to_double();
  double atol = params[1].to_double();
  return ptrHead(new Head_Func_IsClose(rtol, atol));
}



static Column op_isclose(Column&& xcol, Column&& ycol,
                         double rtol, double atol)
{
  SType stype1 = xcol.stype();
  SType stype2 = ycol.stype();
  SType stype0 = common_stype(stype1, stype2);
  LType ltype0 = stype_to_ltype(stype0);
  if (ltype0 == LType::BOOL || ltype0 == LType::INT) {
    stype0 = SType::FLOAT64;
  }
  else if (ltype0 != LType::REAL) {
    throw TypeError() << "Cannot apply function `isclose()` to columns with "
        "types `" << stype1 << "` and `" << stype2 << "`";
  }
  if (stype1 != stype0) xcol.cast_inplace(stype0);
  if (stype2 != stype0) ycol.cast_inplace(stype0);
  size_t nrows = xcol.nrows();

  return (stype0 == SType::FLOAT32)
      ? Column(new IsClose_ColumnImpl<float>(std::move(xcol), std::move(ycol),
                                             static_cast<float>(rtol),
                                             static_cast<float>(atol), nrows))
      : Column(new IsClose_ColumnImpl<double>(std::move(xcol), std::move(ycol),
                                              rtol, atol, nrows));
}


Workframe Head_Func_IsClose::evaluate_n(
            const vecExpr& args, EvalContext& ctx) const
{
  xassert(args.size() == 2);
  Workframe lhs = args[0]->evaluate_n(ctx);
  Workframe rhs = args[1]->evaluate_n(ctx);
  if (lhs.ncols() == 1) lhs.repeat_column(rhs.ncols());
  if (rhs.ncols() == 1) rhs.repeat_column(lhs.ncols());
  if (lhs.ncols() != rhs.ncols()) {
    throw ValueError() << "Incompatible column vectors in `isclose()`: "
      "LHS contains " << lhs.ncols() << " items, while RHS has " << rhs.ncols()
      << " items";
  }
  lhs.sync_grouping_mode(rhs);
  auto gmode = lhs.get_grouping_mode();
  Workframe outputs(ctx);
  for (size_t i = 0; i < lhs.ncols(); ++i) {
    Column lhscol = lhs.retrieve_column(i);
    Column rhscol = rhs.retrieve_column(i);
    Column rescol = op_isclose(std::move(lhscol), std::move(rhscol),
                               rtol_, atol_);
    outputs.add_column(std::move(rescol), std::string(), gmode);
  }
  return outputs;
}




}}  // dt::expr
//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------
namespace py {

/**
  * Python-facing function that implements `isclose()`.
  */
static oobj pyfn_isclose(const XArgs& args) {
  const Arg& arg_x    = args[0];
  const Arg& arg_y    = args[1];
  const Arg& arg_rtol = args[2];
  const Arg& arg_atol = args[3];

  if (arg_x.is_none_or_undefined() || arg_y.is_none_or_undefined()) {
    throw TypeError() << "Function `isclose()` requires 2 positional arguments";
  }

  double rtol = arg_rtol.to<double>(1e-5);
  if (rtol < 0 || std::isnan(rtol)) {
    throw ValueError()
        << "Parameter `rtol` in function `isclose()` should be non-negative";
  }

  double atol = arg_atol.to<double>(1e-8);
  if (atol < 0 || std::isnan(atol)) {
    throw ValueError()
        << "Parameter `atol` in function `isclose()` should be non-negative";
  }

  return robj(Expr_Type).call({
      oint(static_cast<size_t>(dt::expr::Op::ISCLOSE)),
      otuple{ arg_x.to_robj(), arg_y.to_robj() },
      otuple{ ofloat(rtol), ofloat(atol) }
  });
}

DECLARE_PYFN(&pyfn_isclose)
    ->name("isclose")
    ->docs(dt::doc_math_isclose)
    ->n_positional_args(2)
    ->n_keyword_args(2)
    ->arg_names({"x", "y", "rtol", "atol"});




}
