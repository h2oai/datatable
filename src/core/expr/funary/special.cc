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
#include <cmath>
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
#include "expr/funary/umaker_impl.h"
#include "ltype.h"
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
    return umaker1<double, double>::make(fn64, SType::AUTO, SType::FLOAT64);
  }
  if (stype == SType::FLOAT32) {
    return umaker1<float, float>::make(fn32, SType::AUTO, SType::FLOAT32);
  }
  if (stype == SType::BOOL || stype_to_ltype(stype) == LType::INT) {
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

Error function ``erf(x)``, which is defined as the integral

.. math::

    \operatorname{erf}(x) = \frac{2}{\sqrt{\tau}} \int^{x/\sqrt{2}}_0 e^{-\frac12 t^2}dt

This function is used in computing probabilities arising from the normal
distribution.

See also
--------
- :func:`erfc(x) <datatable.math.erfc>` -- complimentary error function.
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

Complementary error function ``erfc(x) = 1 - erf(x)``.

The complementary error function is defined as the integral

.. math::

    \operatorname{erfc}(x) = \frac{2}{\sqrt{\tau}} \int^{\infty}_{x/\sqrt{2}} e^{-\frac12 t^2}dt

Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
suffers catastrophic loss of precision at large values of `x`. This
function, however, does not have such a drawback.

See also
--------
- :func:`erf(x) <datatable.math.erf>` -- the error function.
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

The gamma function is defined for all ``x`` except for the negative
integers. For positive ``x`` it can be computed via the integral

.. math::
    \Gamma(x) = \int_0^\infty t^{x-1}e^{-t}dt

For negative ``x`` it can be computed as

.. math::
    \Gamma(x) = \frac{\Gamma(x + k)}{x(x+1)\cdot...\cdot(x+k-1)}

where :math:`k` is any integer such that :math:`x+k` is positive.

If `x` is a positive integer, then :math:`\Gamma(x) = (x - 1)!`.

See also
--------
- :func:`lgamma(x) <datatable.math.lgamma>` -- log-gamma function.
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

Natural logarithm of the absolute value of the Euler Gamma
function of `x`.
)";

py::PKArgs args_lgamma(1, 0, 0, false, false, {"x"}, "lgamma", doc_lgamma);

umaker_ptr resolve_op_lgamma(SType stype) {
  return _resolve_special(stype, "lgamma", &std::lgamma, &std::lgamma);
}




}}  // namespace dt::expr
