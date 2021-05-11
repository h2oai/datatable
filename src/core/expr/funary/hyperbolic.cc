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
  * All standard hyperbolic functions have the same signature:
  *
  *     VOID -> VOID
  *     {BOOL, INT*, FLOAT64} -> FLOAT64
  *     FLOAT32 -> FLOAT32
  *
  */
static umaker_ptr _resolve_hyp(SType stype, const char* name,
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
// Op::SINH
//------------------------------------------------------------------------------

static const char* doc_sinh =
R"(sinh(x)
--

Hyperbolic sine of `x`, defined as
:math:`\sinh x = \frac12(e^x - e^{-x})`.

See also
--------
- :func:`cosh` -- hyperbolic cosine;
- :func:`arsinh` -- inverse hyperbolic sine.
)";

py::PKArgs args_sinh(1, 0, 0, false, false, {"x"}, "sinh", doc_sinh);


umaker_ptr resolve_op_sinh(SType stype) {
  return _resolve_hyp(stype, "sinh", &std::sinh, &std::sinh);
}




//------------------------------------------------------------------------------
// Op::COSH
//------------------------------------------------------------------------------

static const char* doc_cosh =
R"(cosh(x)
--

The hyperbolic cosine of `x`, defined as
:math:`\cosh x = \frac12(e^x + e^{-x})`.

See also
--------
- :func:`sinh` -- hyperbolic sine;
- :func:`arcosh` -- inverse hyperbolic cosine.
)";

py::PKArgs args_cosh(1, 0, 0, false, false, {"x"}, "cosh", doc_cosh);


umaker_ptr resolve_op_cosh(SType stype) {
  return _resolve_hyp(stype, "cosh", &std::cosh, &std::cosh);
}




//------------------------------------------------------------------------------
// Op::TANH
//------------------------------------------------------------------------------

static const char* doc_tanh =
R"(tanh(x)
--

Hyperbolic tangent of `x`, defined as
:math:`\tanh x = \frac{\sinh x}{\cosh x} = \frac{e^x-e^{-x}}{e^x+e^{-x}}`.

See also
--------
- :func:`artanh` -- inverse hyperbolic tangent.
)";

py::PKArgs args_tanh(1, 0, 0, false, false, {"x"}, "tanh", doc_tanh);


umaker_ptr resolve_op_tanh(SType stype) {
  return _resolve_hyp(stype, "tanh", &std::tanh, &std::tanh);
}




//------------------------------------------------------------------------------
// Op::ARSINH
//------------------------------------------------------------------------------

static const char* doc_arsinh =
R"(arsinh(x)
--

The inverse hyperbolic sine of `x`.

This function satisfies the property that ``sinh(arcsinh(x)) == x``.
Alternatively, this function can also be computed as
:math:`\sinh^{-1}(x) = \ln(x + \sqrt{x^2 + 1})`.

See also
--------
- :func:`sinh` -- hyperbolic sine;
- :func:`arcosh` -- inverse hyperbolic cosine.
)";

py::PKArgs args_arsinh(1, 0, 0, false, false, {"x"}, "arsinh", doc_arsinh);


umaker_ptr resolve_op_arsinh(SType stype) {
  return _resolve_hyp(stype, "arsinh", &std::asinh, &std::asinh);
}




//------------------------------------------------------------------------------
// Op::ARCOSH
//------------------------------------------------------------------------------

static const char* doc_arcosh =
R"(arcosh(x)
--

The inverse hyperbolic cosine of `x`.

This function satisfies the property that ``cosh(arccosh(x)) == x``.
Alternatively, this function can also be computed as
:math:`\cosh^{-1}(x) = \ln(x + \sqrt{x^2 - 1})`.

See also
--------
- :func:`cosh` -- hyperbolic cosine;
- :func:`arsinh` -- inverse hyperbolic sine.
)";

py::PKArgs args_arcosh(1, 0, 0, false, false, {"x"}, "arcosh", doc_arcosh);


umaker_ptr resolve_op_arcosh(SType stype) {
  return _resolve_hyp(stype, "arcosh", &std::acosh, &std::acosh);
}




//------------------------------------------------------------------------------
// Op::ARTANH
//------------------------------------------------------------------------------

static const char* doc_artanh =
R"(artanh(x)
--

The inverse hyperbolic tangent of `x`.

This function satisfies the property that ``tanh(artanh(x)) == x``.
Alternatively, this function can also be computed as
:math:`\tanh^{-1}(x) = \frac12\ln\frac{1+x}{1-x}`.

See also
--------
- :func:`tanh` -- hyperbolic tangent;
)";

py::PKArgs args_artanh(1, 0, 0, false, false, {"x"}, "artanh", doc_artanh);


umaker_ptr resolve_op_artanh(SType stype) {
  return _resolve_hyp(stype, "artanh", &std::atanh, &std::atanh);
}




}}  // namespace dt::expr
