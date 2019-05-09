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
#include "datatablemodule.h"
#include "types.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// expr_math11
//------------------------------------------------------------------------------

expr_math11::expr_math11(pexpr&& a, Op op)
  : arg(std::move(a)), opcode(op) {}


SType expr_math11::resolve(const workframe& wf) {
  (void) arg->resolve(wf);
  return SType::FLOAT64;
}


GroupbyMode expr_math11::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


colptr expr_math11::evaluate_eager(workframe& wf) {
  auto arg_res = arg->evaluate_eager(wf);
  Column* col = arg_res.get();
  size_t nrows = col->nrows;

  // return colptr(unaryop(opcode, arg_res.get()));
}



}}  // namespace dt::expr


using ufunc_t = double(*)(double);

struct fninfo {
  dt::expr::Op opcode;
  const char* name;
  ufunc_t corefn;
};

static std::unordered_map<const py::PKArgs*, fninfo> fninfos;

static py::oobj make_pyexpr(dt::expr::Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), arg });
}


//------------------------------------------------------------------------------
// Trigonometric/hyperbolic functions
//------------------------------------------------------------------------------

static py::PKArgs args_acos(
  1, 0, 0, false, false, {"x"}, "arccos",
R"(Inverse trigonometric cosine of x.

The returned value is in the interval [0, pi], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
cos() in the sense that `cos(arccos(x)) == x`.
)");

static py::PKArgs args_acosh(
  1, 0, 0, false, false, {"x"}, "arccosh",
R"(Inverse hyperbolic cosine of x.)");

static py::PKArgs args_asin(
  1, 0, 0, false, false, {"x"}, "arcsin",
R"(Inverse trigonometric sine of x.

The returned value is in the interval [-pi/2, pi/2], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
sin() in the sense that `sin(arcsin(x)) == x`.
)");

static py::PKArgs args_asinh(
  1, 0, 0, false, false, {"x"}, "arcsinh",
R"(Inverse hyperbolic sine of x.)");

static py::PKArgs args_atan(
  1, 0, 0, false, false, {"x"}, "arctan",
R"(Inverse trigonometric tangent of x.)");

static py::PKArgs args_atanh(
  1, 0, 0, false, false, {"x"}, "arctanh",
R"(Inverse hyperbolic tangent of x.)");

static py::PKArgs args_cos(
  1, 0, 0, false, false, {"x"}, "cos", "Trigonometric cosine of x.");

static py::PKArgs args_cosh(
  1, 0, 0, false, false, {"x"}, "cosh", "Hyperbolic cosine of x.");

static py::PKArgs args_sin(
  1, 0, 0, false, false, {"x"}, "sin", "Trigonometric sine of x.");

static py::PKArgs args_sinh(
  1, 0, 0, false, false, {"x"}, "sinh", "Hyperbolic sine of x.");

static py::PKArgs args_tan(
  1, 0, 0, false, false, {"x"}, "tan", "Trigonometric tangent of x.");

static py::PKArgs args_tanh(
  1, 0, 0, false, false, {"x"}, "tanh", "Hyperbolic tangent of x.");

static py::PKArgs args_rad2deg(
  1, 0, 0, false, false, {"x"}, "rad2deg",
"Convert angle measured in radians into degrees.");

static py::PKArgs args_deg2rad(
  1, 0, 0, false, false, {"x"}, "deg2rad",
"Convert angle measured in degrees into radians.");



//------------------------------------------------------------------------------
// Power/exponent functions
//------------------------------------------------------------------------------

static py::PKArgs args_cbrt(
  1, 0, 0, false, false, {"x"}, "cbrt", "Cubic root of x.");

static py::PKArgs args_exp(
  1, 0, 0, false, false, {"x"}, "exp",
"The Euler's constant (e = 2.71828...) raised to the power of x.");

static py::PKArgs args_exp2(
  1, 0, 0, false, false, {"x"}, "exp2", "Compute 2 raised to the power of x.");

static py::PKArgs args_expm1(
  1, 0, 0, false, false, {"x"}, "expm1",
R"(Compute e raised to the power of x, minus 1. This function is
equivalent to `exp(x) - 1`, but it is more accurate for arguments
`x` close to zero.)");

static py::PKArgs args_log(
  1, 0, 0, false, false, {"x"}, "log", "Natural logarithm of x.");

static py::PKArgs args_log10(
  1, 0, 0, false, false, {"x"}, "log10", "Base-10 logarithm of x.");

static py::PKArgs args_log1p(
  1, 0, 0, false, false, {"x"}, "log1p", "Natural logarithm of (1 + x).");

static py::PKArgs args_log2(
  1, 0, 0, false, false, {"x"}, "log2", "Base-2 logarithm of x.");

static py::PKArgs args_sqrt(
  1, 0, 0, false, false, {"x"}, "sqrt", "Square root of x.");



//------------------------------------------------------------------------------
// Special mathematical functions
//------------------------------------------------------------------------------

static py::PKArgs args_erf(
  1, 0, 0, false, false, {"x"}, "erf",
R"(Error function erf(x).

The error function is defined as the integral
  erf(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, 0, x}]
)");

static py::PKArgs args_erfc(
  1, 0, 0, false, false, {"x"}, "erfc",
R"(Complementary error function `erfc(x) = 1 - erf(x)`.

The complementary error function is defined as the integral
  erfc(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, x, +inf}]

Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
suffers catastrophic loss of precision at large values of `x`. This
function, however, does not have such drawback.
)");

static py::PKArgs args_gamma(
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

static py::PKArgs args_lgamma(
  1, 0, 0, false, false, {"x"}, "lgamma",
R"(Natural logarithm of absolute value of gamma function of x.)");



static py::oobj mathfn_11(const py::PKArgs& args) {
  xassert(fninfos.count(&args) == 1);
  fninfo& fn = fninfos[&args];

  py::robj arg = args[0].to_robj();
  if (!arg) {
    throw TypeError() << '`' << fn.name << "()` takes exactly one "
        "argument, 0 given";
  }
  if (arg.is_dtexpr()) {
    return make_pyexpr(fn.opcode, arg);
  }
  if (arg.is_float() || arg.is_int()) {
    return py::ofloat(fn.corefn(arg.to_double()));
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



void py::DatatableModule::init_methods_math() {
  ADD_FN(&mathfn_11, args_acos);
  ADD_FN(&mathfn_11, args_acosh);
  ADD_FN(&mathfn_11, args_asin);
  ADD_FN(&mathfn_11, args_asinh);
  ADD_FN(&mathfn_11, args_atan);
  ADD_FN(&mathfn_11, args_atanh);
  ADD_FN(&mathfn_11, args_cos);
  ADD_FN(&mathfn_11, args_cosh);
  ADD_FN(&mathfn_11, args_deg2rad);
  ADD_FN(&mathfn_11, args_rad2deg);
  ADD_FN(&mathfn_11, args_sin);
  ADD_FN(&mathfn_11, args_sinh);
  ADD_FN(&mathfn_11, args_tan);
  ADD_FN(&mathfn_11, args_tanh);

  ADD_FN(&mathfn_11, args_cbrt);
  ADD_FN(&mathfn_11, args_exp);
  ADD_FN(&mathfn_11, args_exp2);
  ADD_FN(&mathfn_11, args_expm1);
  ADD_FN(&mathfn_11, args_log);
  ADD_FN(&mathfn_11, args_log10);
  ADD_FN(&mathfn_11, args_log1p);
  ADD_FN(&mathfn_11, args_log2);
  ADD_FN(&mathfn_11, args_sqrt);

  ADD_FN(&mathfn_11, args_erf);
  ADD_FN(&mathfn_11, args_erfc);
  ADD_FN(&mathfn_11, args_gamma);
  ADD_FN(&mathfn_11, args_lgamma);

  using namespace dt::expr;
  fninfos[&args_acos]    = {Op::ARCCOS,  "arccos",  &std::acos};
  fninfos[&args_acosh]   = {Op::ARCCOSH, "arccosh", &std::acosh};
  fninfos[&args_asin]    = {Op::ARCSIN,  "arcsin",  &std::asin};
  fninfos[&args_asinh]   = {Op::ARCSINH, "arcsinh", &std::asinh};
  fninfos[&args_atan]    = {Op::ARCTAN,  "arctan",  &std::atan};
  fninfos[&args_atanh]   = {Op::ARCTANH, "arctanh", &std::atanh};
  fninfos[&args_cos]     = {Op::COS,     "cos",     &std::cos};
  fninfos[&args_cosh]    = {Op::COSH,    "cosh",    &std::cosh};
  fninfos[&args_deg2rad] = {Op::DEG2RAD, "deg2rad", &fn_deg2rad};
  fninfos[&args_rad2deg] = {Op::RAD2DEG, "rad2deg", &fn_rad2deg};
  fninfos[&args_sin]     = {Op::SIN,     "sin",     &std::sin};
  fninfos[&args_sinh]    = {Op::SINH,    "sinh",    &std::sinh};
  fninfos[&args_tan]     = {Op::TAN,     "tan",     &std::tan};
  fninfos[&args_tanh]    = {Op::TANH,    "tanh",    &std::tanh};

  fninfos[&args_cbrt]    = {Op::CBRT,    "cbrt",    &std::cbrt};
  fninfos[&args_exp]     = {Op::EXP,     "exp",     &std::exp};
  fninfos[&args_exp2]    = {Op::EXP2,    "exp2",    &std::exp2};
  fninfos[&args_exp2]    = {Op::EXPM1,   "expm1",   &std::expm1};
  fninfos[&args_log]     = {Op::LOG,     "log",     &std::log};
  fninfos[&args_log10]   = {Op::LOG10,   "log10",   &std::log10};
  fninfos[&args_log1p]   = {Op::LOG1P,   "log1p",   &std::log1p};
  fninfos[&args_log2]    = {Op::LOG2,    "log2",    &std::log2};
  fninfos[&args_sqrt]    = {Op::SQRT,    "sqrt",    &std::sqrt};

  fninfos[&args_erf]     = {Op::ERF,     "erf",     &std::erf};
  fninfos[&args_erfc]    = {Op::ERFC,    "erfc",    &std::erfc};
  fninfos[&args_gamma]   = {Op::GAMMA,   "gamma",   &std::tgamma};
  fninfos[&args_lgamma]  = {Op::LGAMMA,  "lgamma",  &std::lgamma};  // Check thread-safety...
}
