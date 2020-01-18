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
#include "column/isclose.h"
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "python/tuple.h"
#include "utils/assert.h"
#include "column.h"
#include "datatablemodule.h"
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
  LType ltype0 = ::info(stype0).ltype();
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
            const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 2);
  Workframe lhs = args[0].evaluate_n(ctx);
  Workframe rhs = args[1].evaluate_n(ctx);
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


static const char* doc_isclose =
R"(isclose(x, y, *, rtol=1e-5, atol=1e-8)
--

Compare two numbers x and y, and return True if they are close
within the requested relative/absolute tolerance. This function
only returns True/False, never NA.

More specifically, isclose(x, y) is True if either of the following
are true:
  - ``x == y`` (including the case when x and y are NAs),
  - ``abs(x - y) <= atol + rtol * abs(y)`` and neither x nor y are NA

The tolerance parameters ``rtol``, ``atol`` must be positive floats,
and cannot be expressions.
)";

static py::PKArgs args_isclose(
    2, 0, 2, false, false, {"x", "y", "rtol", "atol"}, "isclose", doc_isclose);




static oobj make_pyexpr(dt::expr::Op opcode, otuple targs, otuple tparams) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), targs, tparams });
}


/**
  * Python-facing function that implements `isclose()`.
  */
static oobj pyfn_isclose(const PKArgs& args)
{
  const Arg& arg_x = args[0];
  const Arg& arg_y = args[1];
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

  return make_pyexpr(dt::expr::Op::ISCLOSE,
                     otuple{ arg_x.to_robj(), arg_y.to_robj() },
                     otuple{ ofloat(rtol), ofloat(atol) });
}



void DatatableModule::init_methods_isclose() {
  ADD_FN(&pyfn_isclose, args_isclose);
}



}
