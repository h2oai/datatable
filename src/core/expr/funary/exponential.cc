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
#include <cmath>
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
#include "expr/funary/umaker_impl.h"
namespace dt {
namespace expr {


using func32_t = float(*)(float);
using func64_t = double(*)(double);

/**
  * All exponential functions have the same signature:
  *
  *     VOID -> VOID
  *     {BOOL, INT*, FLOAT64} -> FLOAT64
  *     FLOAT32 -> FLOAT32
  *
  */
static umaker_ptr _resolve_exp(SType stype, const char* name,
                               func32_t fn32, func64_t fn64)
{
  if (stype == SType::VOID) {
    return umaker_ptr(new umaker_copy());
  }
  if (stype == SType::FLOAT64) {
    return umaker1<double, double>::make(fn64, SType::VOID, SType::FLOAT64);
  }
  if (stype == SType::FLOAT32) {
    return umaker1<float, float>::make(fn32, SType::VOID, SType::FLOAT32);
  }
  if (stype == SType::BOOL || ::info(stype).ltype() == LType::INT) {
    return umaker1<double, double>::make(fn64, SType::FLOAT64, SType::FLOAT64);
  }
  throw TypeError() << "Function `" << name << "` cannot be applied to a "
                       "column of type `" << stype << "`";
}




//------------------------------------------------------------------------------
// Op::CBRT
//------------------------------------------------------------------------------

static const char* doc_cbrt =
R"(cbrt(x)
--

Cubic root of x.
)";

py::PKArgs args_cbrt(1, 0, 0, false, false, {"x"}, "cbrt", doc_cbrt);


umaker_ptr resolve_op_cbrt(SType stype) {
  return _resolve_exp(stype, "cbrt", &std::cbrt, &std::cbrt);
}




//------------------------------------------------------------------------------
// Op::EXP
//------------------------------------------------------------------------------

static const char* doc_exp =
R"(exp(x)
--

The exponent of x, that is e raised to the power of x. Here "e" is
the Euler's constant (e = 2.71828...).
)";

py::PKArgs args_exp(1, 0, 0, false, false, {"x"}, "exp", doc_exp);

umaker_ptr resolve_op_exp(SType stype) {
  return _resolve_exp(stype, "exp", &std::exp, &std::exp);
}




//------------------------------------------------------------------------------
// Op::EXP2
//------------------------------------------------------------------------------

static const char* doc_exp2 =
R"(exp2(x)
--

Binary exponent of x, same as 2**x.
)";

py::PKArgs args_exp2(1, 0, 0, false, false, {"x"}, "exp2", doc_exp2);

umaker_ptr resolve_op_exp2(SType stype) {
  return _resolve_exp(stype, "exp2", &std::exp2, &std::exp2);
}




//------------------------------------------------------------------------------
// Op::EXPM1
//------------------------------------------------------------------------------

static const char* doc_expm1 =
R"(expm1(x)
--

The exponent of x minus 1. This function is equivalent to
`exp(x) - 1`, but it is more accurate for arguments `x` close to
zero.
)";

py::PKArgs args_expm1(1, 0, 0, false, false, {"x"}, "expm1", doc_expm1);

umaker_ptr resolve_op_expm1(SType stype) {
  return _resolve_exp(stype, "expm1", &std::expm1, &std::expm1);
}




//------------------------------------------------------------------------------
// Op::LOG
//------------------------------------------------------------------------------

static const char* doc_log =
R"(log(x)
--

Natural logarithm of x.
)";

py::PKArgs args_log(1, 0, 0, false, false, {"x"}, "log", doc_log);

umaker_ptr resolve_op_log(SType stype) {
  return _resolve_exp(stype, "log", &std::log, &std::log);
}




//------------------------------------------------------------------------------
// Op::LOG10
//------------------------------------------------------------------------------

static const char* doc_log10 =
R"(log10(x)
--

Decimal (base-10) logarithm of x.
)";

py::PKArgs args_log10(1, 0, 0, false, false, {"x"}, "log10", doc_log10);

umaker_ptr resolve_op_log10(SType stype) {
  return _resolve_exp(stype, "log10", &std::log10, &std::log10);
}




//------------------------------------------------------------------------------
// Op::LOG1P
//------------------------------------------------------------------------------

static const char* doc_log1p =
R"(log1p(x)
--

Natural logarithm of (1 + x). This function has improved numeric
precision for small values of x.
)";

py::PKArgs args_log1p(1, 0, 0, false, false, {"x"}, "log1p", doc_log1p);

umaker_ptr resolve_op_log1p(SType stype) {
  return _resolve_exp(stype, "log1p", &std::log1p, &std::log1p);
}




//------------------------------------------------------------------------------
// Op::LOG2
//------------------------------------------------------------------------------

static const char* doc_log2 =
R"(log2(x)
--

Binary (base-2) logarithm of x.
)";

py::PKArgs args_log2(1, 0, 0, false, false, {"x"}, "log2", doc_log2);

umaker_ptr resolve_op_log2(SType stype) {
  return _resolve_exp(stype, "log2", &std::log2, &std::log2);
}




//------------------------------------------------------------------------------
// Op::SQRT
//------------------------------------------------------------------------------

static const char* doc_sqrt =
R"(sqrt(x)
--

The square root of x, same as ``x ** 0.5``.
)";

py::PKArgs args_sqrt(1, 0, 0, false, false, {"x"}, "sqrt", doc_sqrt);

umaker_ptr resolve_op_sqrt(SType stype) {
  return _resolve_exp(stype, "sqrt", &std::sqrt, &std::sqrt);
}




//------------------------------------------------------------------------------
// Op::SQUARE
//------------------------------------------------------------------------------

static const char* doc_square =
R"(square(x)
--

The square of x, same as ``x ** 2.0``. As with all other math
functions, the result is floating-point, even if the argument
x is integer.
)";

py::PKArgs args_square(1, 0, 0, false, false, {"x"}, "square", doc_square);


static float fn_square(float x) {
  return x * x;
}
static double fn_square(double x) {
  return x * x;
}


umaker_ptr resolve_op_square(SType stype) {
  return _resolve_exp(stype, "square", &fn_square, &fn_square);
}




}}  // namespace dt::expr
