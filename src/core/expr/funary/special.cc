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
  * All special math functions have the same signature:
  *
  *     VOID -> VOID
  *     {BOOL, INT*, FLOAT64} -> FLOAT64
  *     FLOAT32 -> FLOAT32
  *
  */
static umaker_ptr _resolve_special(SType stype, const char* name,
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
// Op::ERF
//------------------------------------------------------------------------------

static const char* doc_erf =
R"(erf(x)
--

Error function erf(x), which is defined as the integral

  erf(x) = sqrt(8/tau) * Integrate[exp(-t**2), {t, 0, x}]

)";

py::PKArgs args_erf(1, 0, 0, false, false, {"x"}, "erf", doc_erf);

umaker_ptr resolve_op_erf(SType stype) {
  return _resolve_special(stype, "erf", &std::erf, &std::erf);
}




//------------------------------------------------------------------------------
// Op::ERFC
//------------------------------------------------------------------------------

static const char* doc_erfc =
R"(erfc(x)
--

Complementary error function `erfc(x) = 1 - erf(x)`.

The complementary error function is defined as the integral
  erfc(x) = sqrt(8/tau) * Integrate[exp(-t**2), {t, x, +inf}]

Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
suffers catastrophic loss of precision at large values of `x`. This
function, however, does not have such drawback.
)";

py::PKArgs args_erfc(1, 0, 0, false, false, {"x"}, "erfc", doc_erfc);

umaker_ptr resolve_op_erfc(SType stype) {
  return _resolve_special(stype, "erfc", &std::erfc, &std::erfc);
}




//------------------------------------------------------------------------------
// Op::GAMMA
//------------------------------------------------------------------------------

static const char* doc_gamma =
R"(gamma(x)
--

Euler Gamma function of x.

The gamma function is defined for all positive `x` as the integral
  gamma(x) = Integrate[t**(x-1) * exp(-t), {t, 0, +inf}]

In addition, for non-integer negative `x` the function is defined
via the relationship
  gamma(x) = gamma(x + k)/[x*(x+1)*...*(x+k-1)]
  where k = ceil(|x|)

If `x` is a positive integer, then `gamma(x) = (x - 1)!`.
)";

py::PKArgs args_gamma(1, 0, 0, false, false, {"x"}, "gamma", doc_gamma);

umaker_ptr resolve_op_gamma(SType stype) {
  return _resolve_special(stype, "gamma", &std::tgamma, &std::tgamma);
}




//------------------------------------------------------------------------------
// Op::LGAMMA
//------------------------------------------------------------------------------

static const char* doc_lgamma =
R"(lgamma(x)
--

Natural logarithm of the absolute value of gamma function of x.
)";

py::PKArgs args_lgamma(1, 0, 0, false, false, {"x"}, "lgamma", doc_lgamma);

umaker_ptr resolve_op_lgamma(SType stype) {
  return _resolve_special(stype, "lgamma", &std::lgamma, &std::lgamma);
}




}}  // namespace dt::expr
