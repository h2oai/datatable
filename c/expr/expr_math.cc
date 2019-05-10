//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <cmath>
#include <unordered_map>
#include "expr/expr.h"
#include "expr/expr_math.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "datatablemodule.h"
#include "types.h"
namespace dt {
namespace expr {

using ufunc_t = double(*)(double);
static std::unordered_map<Op, ufunc_t> fn11s;


//------------------------------------------------------------------------------
// expr_math11
//------------------------------------------------------------------------------

expr_math11::expr_math11(pexpr&& a, Op op)
  : arg(std::move(a)), opcode(op) {}


SType expr_math11::resolve(const workframe& wf) {
  auto arg_stype = arg->resolve(wf);
  if (!is_numeric(arg_stype)) {
    throw TypeError() << "Cannot apply function `` to a column of type "
        << arg_stype;
  }
  return SType::FLOAT64;
}


GroupbyMode expr_math11::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


colptr expr_math11::evaluate_eager(workframe& wf) {
  colptr input_column = arg->evaluate_eager(wf);
  if (input_column->stype() != SType::FLOAT64) {
    input_column = colptr(input_column->cast(SType::FLOAT64));
  }
  input_column->materialize();

  size_t nrows = input_column->nrows;
  const MemoryRange& input_mbuf = input_column->data_buf();
  auto inp = static_cast<const double*>(input_mbuf.rptr());

  // If the input column's data buffer "is_writable", then we're
  // dealing with a temporary column that was just created from `arg`.
  // In this case, instead of creating data buffer for the resulting
  // column, we just reuse the data buffer of the input. This works,
  // because we need to read each value in the input only once, and
  // then we can replace it with the output value immediately.
  MemoryRange output_mbuf;
  double* out = nullptr;
  if (input_mbuf.is_writable()) {
    out = static_cast<double*>(input_mbuf.xptr());
    output_mbuf = MemoryRange(input_mbuf);
  } else {
    output_mbuf = MemoryRange::mem(sizeof(double) * nrows);
    out = static_cast<double*>(output_mbuf.xptr());
  }
  colptr output_column = colptr(
      Column::new_mbuf_column(SType::FLOAT64, std::move(output_mbuf)));

  ufunc_t f = fn11s[opcode];

  parallel_for_static(nrows,
    [&](size_t i) {
      out[i] = f(inp[i]);
    });

  return output_column;
}



}}  // namespace dt::expr
namespace py {



struct fninfo {
  dt::expr::Op opcode;
  const char* name;
  dt::expr::ufunc_t corefn;
};

static std::unordered_map<const PKArgs*, fninfo> fninfos;


//------------------------------------------------------------------------------
// Trigonometric/hyperbolic functions
//------------------------------------------------------------------------------

static PKArgs args_acos(
  1, 0, 0, false, false, {"x"}, "arccos",
R"(Inverse trigonometric cosine of x.

The returned value is in the interval [0, pi], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
cos() in the sense that `cos(arccos(x)) == x`.
)");

static PKArgs args_acosh(
  1, 0, 0, false, false, {"x"}, "arccosh",
R"(Inverse hyperbolic cosine of x.)");

static PKArgs args_asin(
  1, 0, 0, false, false, {"x"}, "arcsin",
R"(Inverse trigonometric sine of x.

The returned value is in the interval [-pi/2, pi/2], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
sin() in the sense that `sin(arcsin(x)) == x`.
)");

static PKArgs args_asinh(
  1, 0, 0, false, false, {"x"}, "arcsinh",
R"(Inverse hyperbolic sine of x.)");

static PKArgs args_atan(
  1, 0, 0, false, false, {"x"}, "arctan",
R"(Inverse trigonometric tangent of x.)");

static PKArgs args_atanh(
  1, 0, 0, false, false, {"x"}, "arctanh",
R"(Inverse hyperbolic tangent of x.)");

static PKArgs args_cos(
  1, 0, 0, false, false, {"x"}, "cos", "Trigonometric cosine of x.");

static PKArgs args_cosh(
  1, 0, 0, false, false, {"x"}, "cosh", "Hyperbolic cosine of x.");

static PKArgs args_sin(
  1, 0, 0, false, false, {"x"}, "sin", "Trigonometric sine of x.");

static PKArgs args_sinh(
  1, 0, 0, false, false, {"x"}, "sinh", "Hyperbolic sine of x.");

static PKArgs args_tan(
  1, 0, 0, false, false, {"x"}, "tan", "Trigonometric tangent of x.");

static PKArgs args_tanh(
  1, 0, 0, false, false, {"x"}, "tanh", "Hyperbolic tangent of x.");

static PKArgs args_rad2deg(
  1, 0, 0, false, false, {"x"}, "rad2deg",
"Convert angle measured in radians into degrees.");

static PKArgs args_deg2rad(
  1, 0, 0, false, false, {"x"}, "deg2rad",
"Convert angle measured in degrees into radians.");



//------------------------------------------------------------------------------
// Power/exponent functions
//------------------------------------------------------------------------------

static PKArgs args_cbrt(
  1, 0, 0, false, false, {"x"}, "cbrt", "Cubic root of x.");

static PKArgs args_exp(
  1, 0, 0, false, false, {"x"}, "exp",
"The Euler's constant (e = 2.71828...) raised to the power of x.");

static PKArgs args_exp2(
  1, 0, 0, false, false, {"x"}, "exp2", "Compute 2 raised to the power of x.");

static PKArgs args_expm1(
  1, 0, 0, false, false, {"x"}, "expm1",
R"(Compute e raised to the power of x, minus 1. This function is
equivalent to `exp(x) - 1`, but it is more accurate for arguments
`x` close to zero.)");

static PKArgs args_log(
  1, 0, 0, false, false, {"x"}, "log", "Natural logarithm of x.");

static PKArgs args_log10(
  1, 0, 0, false, false, {"x"}, "log10", "Base-10 logarithm of x.");

static PKArgs args_log1p(
  1, 0, 0, false, false, {"x"}, "log1p", "Natural logarithm of (1 + x).");

static PKArgs args_log2(
  1, 0, 0, false, false, {"x"}, "log2", "Base-2 logarithm of x.");

static PKArgs args_sqrt(
  1, 0, 0, false, false, {"x"}, "sqrt", "Square root of x.");

static PKArgs args_square(
  1, 0, 0, false, false, {"x"}, "square", "Square of x, i.e. same as x**2.");



//------------------------------------------------------------------------------
// Special mathematical functions
//------------------------------------------------------------------------------

static PKArgs args_erf(
  1, 0, 0, false, false, {"x"}, "erf",
R"(Error function erf(x).

The error function is defined as the integral
  erf(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, 0, x}]
)");

static PKArgs args_erfc(
  1, 0, 0, false, false, {"x"}, "erfc",
R"(Complementary error function `erfc(x) = 1 - erf(x)`.

The complementary error function is defined as the integral
  erfc(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, x, +inf}]

Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
suffers catastrophic loss of precision at large values of `x`. This
function, however, does not have such drawback.
)");

static PKArgs args_gamma(
  1, 0, 0, false, false, {"x"}, "gamma",
R"(Euler Gamma function of x.

The gamma function is defined for all positive `x` as the integral
  gamma(x) = Integrate[t**(x-1) * exp(-t), {t, 0, +inf}]

In addition, for non-integer negative `x` the function is defined
via the relationship
  gamma(x) = gamma(x + k)/[x*(x+1)*...*(x+k-1)]
  where k = ceil(|x|)

If `x` is a positive integer, then `gamma(x) = (x - 1)!`.
)");

static PKArgs args_lgamma(
  1, 0, 0, false, false, {"x"}, "lgamma",
R"(Natural logarithm of absolute value of gamma function of x.)");



//------------------------------------------------------------------------------
// Miscellaneous mathematical functions
//------------------------------------------------------------------------------

static PKArgs args_fabs(
  1, 0, 0, false, false, {"x"}, "fabs",
  "Absolute value of x, returned as float64.");

static PKArgs args_sign(
  1, 0, 0, false, false, {"x"}, "sign",
R"(The sign of x, returned as float64.

This function returns 1 if x is positive (including positive
infinity), -1 if x is negative, 0 if x is zero, and NA if
x is NA.)");


//------------------------------------------------------------------------------
// Implement Python API
//------------------------------------------------------------------------------

static oobj make_pyexpr1(dt::expr::Op opcode, oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), arg });
}

static oobj make_pyexpr2(dt::expr::Op opcode, oobj arg1, oobj arg2) {
  size_t op = static_cast<size_t>(opcode);
  return robj(Expr_Type).call({ oint(op), arg1, arg2 });
}


// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static oobj process_frame(dt::expr::Op opcode, oobj arg) {
  xassert(arg.is_frame());
  Frame* frame = static_cast<Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  olist columns(dt->ncols);
  for (size_t i = 0; i < dt->ncols; ++i) {
    oobj col_selector = make_pyexpr2(dt::expr::Op::COL, oint(0), oint(i));
    columns.set(i, make_pyexpr1(opcode, col_selector));
  }

  oobj res = frame->m__getitem__(otuple{ None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(dt);

  return res;
}


static oobj mathfn_11(const PKArgs& args) {
  xassert(fninfos.count(&args) == 1);
  fninfo& fn = fninfos[&args];

  robj arg = args[0].to_robj();
  if (arg.is_numeric() || arg.is_none()) {
    double res = fn.corefn(arg.to_double());
    return std::isnan(res)? None() : ofloat(res);
  }
  if (arg.is_dtexpr()) {
    return make_pyexpr1(fn.opcode, arg);
  }
  if (arg.is_frame()) {
    return process_frame(fn.opcode, arg);
  }
  if (!arg) {
    throw TypeError() << '`' << fn.name << "()` takes exactly one "
        "argument, 0 given";
  }
  throw TypeError() << '`' << fn.name << "()` cannot be applied to "
      "an argument of type " << arg.typeobj();
}




//------------------------------------------------------------------------------
// Custom core functions
//------------------------------------------------------------------------------

static double fn_rad2deg(double x) {
  static constexpr double DEGREES_IN_RADIAN = 57.295779513082323;
  return x * DEGREES_IN_RADIAN;
}

static double fn_deg2rad(double x) {
  static constexpr double RADIANS_IN_DEGREE = 0.017453292519943295;
  return x * RADIANS_IN_DEGREE;
}

static double fn_square(double x) {
  return x * x;
}

static double fn_sign(double x) {
  return (x > 0)? 1.0 : (x < 0)? -1.0 : (x == 0)? 0.0 : GETNA<double>();
}



#define ADD11(OP, ARGS, NAME, FN)  \
  ADD_FN(&mathfn_11, ARGS);        \
  fninfos[&ARGS] = {OP, NAME, FN}; \
  fn11s[OP] = FN;

void py::DatatableModule::init_methods_math() {
  using namespace dt::expr;
  ADD11(Op::ARCCOS,  args_acos,    "arccos",  &std::acos);
  ADD11(Op::ARCCOSH, args_acosh,   "arccosh", &std::acosh);
  ADD11(Op::ARCSIN,  args_asin,    "arsin",   &std::asin);
  ADD11(Op::ARCSINH, args_asinh,   "arcsinh", &std::asinh);
  ADD11(Op::ARCTAN,  args_atan,    "arctan",  &std::atan);
  ADD11(Op::ARCTANH, args_atanh,   "arctanh", &std::atanh);
  ADD11(Op::COS,     args_cos,     "cos",     &std::cos);
  ADD11(Op::COSH,    args_cosh,    "cosh",    &std::cosh);
  ADD11(Op::DEG2RAD, args_deg2rad, "deg2rad", &fn_deg2rad);
  ADD11(Op::RAD2DEG, args_rad2deg, "rad2deg", &fn_rad2deg);
  ADD11(Op::SIN,     args_sin,     "sin",     &std::sin);
  ADD11(Op::SINH,    args_sinh,    "sinh",    &std::sinh);
  ADD11(Op::TAN,     args_tan,     "tan",     &std::tan);
  ADD11(Op::TANH,    args_tanh,    "tanh",    &std::tanh);

  ADD11(Op::CBRT,    args_cbrt,    "cbrt",    &std::cbrt);
  ADD11(Op::EXP,     args_exp,     "exp",     &std::exp);
  ADD11(Op::EXP2,    args_exp2,    "exp2",    &std::exp2);
  ADD11(Op::EXPM1,   args_exp2,    "expm1",   &std::expm1);
  ADD11(Op::LOG,     args_log,     "log",     &std::log);
  ADD11(Op::LOG10,   args_log10,   "log10",   &std::log10);
  ADD11(Op::LOG1P,   args_log1p,   "log1p",   &std::log1p);
  ADD11(Op::LOG2,    args_log2,    "log2",    &std::log2);
  ADD11(Op::SQRT,    args_sqrt,    "sqrt",    &std::sqrt);
  ADD11(Op::SQUARE,  args_square,  "square",  &fn_square);

  ADD11(Op::ERF,     args_erf,     "erf",     &std::erf);
  ADD11(Op::ERFC,    args_erfc,    "erfc",    &std::erfc);
  ADD11(Op::GAMMA,   args_gamma,   "gamma",   &std::tgamma);
  ADD11(Op::LGAMMA,  args_lgamma,  "lgamma",  &std::lgamma);

  ADD11(Op::FABS,    args_fabs,    "fabs",    &std::fabs);
  ADD11(Op::SIGN,    args_sign,    "sign",    &fn_sign);
}

#undef ADD11

} // namespace py
