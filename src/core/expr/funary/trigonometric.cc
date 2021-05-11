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
  * All standard trigonometric functions have the same signature:
  *
  *     VOID -> VOID
  *     {BOOL, INT*, FLOAT64} -> FLOAT64
  *     FLOAT32 -> FLOAT32
  *
  */
static umaker_ptr _resolve_trig(SType stype, const char* name,
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
// Op::SIN
//------------------------------------------------------------------------------

static const char* doc_sin =
R"(sin(x)
--

Compute the trigonometric sine of angle ``x`` measured in radians.

This function can only be applied to numeric columns (real, integer, or
boolean), and produces a float64 result, except when the argument ``x`` is
float32, in which case the result is float32 as well.

See also
--------
- :func:`cos(x)` -- the trigonometric cosine function;
- :func:`arcsin(x)` -- the inverse sine function.
)";

py::PKArgs args_sin(1, 0, 0, false, false, {"x"}, "sin", doc_sin);


umaker_ptr resolve_op_sin(SType stype) {
  return _resolve_trig(stype, "sin", &std::sin, &std::sin);
}




//------------------------------------------------------------------------------
// Op::COS
//------------------------------------------------------------------------------

static const char* doc_cos =
R"(cos(x)
--

Compute the trigonometric cosine of angle ``x`` measured in radians.

This function can only be applied to numeric columns (real, integer, or
boolean), and produces a float64 result, except when the argument ``x`` is
float32, in which case the result is float32 as well.

See also
--------
- :func:`sin(x)` -- the trigonometric sine function;
- :func:`arccos(x)` -- the inverse cosine function.
)";

py::PKArgs args_cos(1, 0, 0, false, false, {"x"}, "cos", doc_cos);


umaker_ptr resolve_op_cos(SType stype) {
  return _resolve_trig(stype, "cos", &std::cos, &std::cos);
}




//------------------------------------------------------------------------------
// Op::TAN
//------------------------------------------------------------------------------

static const char* doc_tan =
R"(tan(x)
--

Compute the trigonometric tangent of ``x``, which is the ratio
``sin(x)/cos(x)``.

This function can only be applied to numeric columns (real, integer, or
boolean), and produces a float64 result, except when the argument ``x`` is
float32, in which case the result is float32 as well.

See also
--------
- :func:`arctan(x)` -- the inverse tangent function.
)";

py::PKArgs args_tan(1, 0, 0, false, false, {"x"}, "tan", doc_tan);


umaker_ptr resolve_op_tan(SType stype) {
  return _resolve_trig(stype, "tan", &std::tan, &std::tan);
}




//------------------------------------------------------------------------------
// Op::ARCSIN
//------------------------------------------------------------------------------

static const char* doc_arcsin =
R"(arcsin(x)
--

Inverse trigonometric sine of `x`.

In mathematics, this may be written as :math:`\arcsin x` or
:math:`\sin^{-1}x`.

The returned value is in the interval :math:`[-\frac14 \tau, \frac14\tau]`,
and NA for the values of ``x`` that lie outside the interval ``[-1, 1]``.
This function is the inverse of :func:`sin()` in the sense
that `sin(arcsin(x)) == x` for all ``x`` in the interval ``[-1, 1]``.

See also
--------
- :func:`sin(x)` -- the trigonometric sine function;
- :func:`arccos(x)` -- the inverse cosine function.
)";

py::PKArgs args_arcsin(1, 0, 0, false, false, {"x"}, "arcsin", doc_arcsin);


umaker_ptr resolve_op_arcsin(SType stype) {
  return _resolve_trig(stype, "arcsin", &std::asin, &std::asin);
}




//------------------------------------------------------------------------------
// Op::ARCCOS
//------------------------------------------------------------------------------

static const char* doc_arccos =
R"(arccos(x)
--

Inverse trigonometric cosine of `x`.

In mathematics, this may be written as :math:`\arccos x` or
:math:`\cos^{-1}x`.

The returned value is in the interval :math:`[0, \frac12\tau]`,
and NA for the values of ``x`` that lie outside the interval
``[-1, 1]``. This function is the inverse of
:func:`cos()` in the sense that
`cos(arccos(x)) == x` for all ``x`` in the interval ``[-1, 1]``.

See also
--------
- :func:`cos(x)` -- the trigonometric cosine function;
- :func:`arcsin(x)` -- the inverse sine function.
)";

py::PKArgs args_arccos(1, 0, 0, false, false, {"x"}, "arccos", doc_arccos);


umaker_ptr resolve_op_arccos(SType stype) {
  return _resolve_trig(stype, "arccos", &std::acos, &std::acos);
}




//------------------------------------------------------------------------------
// Op::ARCTAN
//------------------------------------------------------------------------------

static const char* doc_arctan =
R"(arctan(x)
--

Inverse trigonometric tangent of `x`.

This function satisfies the property that ``tan(arctan(x)) == x``.

See also
--------
- :func:`atan2(x, y)` -- two-argument inverse tangent function;
- :func:`tan(x)` -- the trigonometric tangent function.
)";

py::PKArgs args_arctan(1, 0, 0, false, false, {"x"}, "arctan", doc_arctan);


umaker_ptr resolve_op_arctan(SType stype) {
  return _resolve_trig(stype, "arctan", &std::atan, &std::atan);
}




//------------------------------------------------------------------------------
// Op::DEG2RAD
//------------------------------------------------------------------------------

static const char* doc_deg2rad =
R"(deg2rad(x)
--

Convert angle measured in degrees into radians:
:math:`\operatorname{deg2rad}(x) = x\cdot\frac{\tau}{360}`.

See also
--------
- :func:`rad2deg(x)` -- convert radians into degrees.
)";

py::PKArgs args_deg2rad(1, 0, 0, false, false, {"x"}, "deg2rad", doc_deg2rad);


static double f64_deg2rad(double x) {
  static constexpr double RADIANS_IN_DEGREE = 0.017453292519943295;
  return x * RADIANS_IN_DEGREE;
}

static float f32_deg2rad(float x) {
  static constexpr float RADIANS_IN_DEGREE = 0.0174532924f;
  return x * RADIANS_IN_DEGREE;
}


umaker_ptr resolve_op_deg2rad(SType stype) {
  return _resolve_trig(stype, "deg2rad", f32_deg2rad, f64_deg2rad);
}




//------------------------------------------------------------------------------
// Op::RAD2DEG
//------------------------------------------------------------------------------

static const char* doc_rad2deg =
R"(rad2deg(x)
--

Convert angle measured in radians into degrees:
:math:`\operatorname{rad2deg}(x) = x\cdot\frac{360}{\tau}`.

See also
--------
- :func:`deg2rad(x)` -- convert degrees into radians.
)";

py::PKArgs args_rad2deg(1, 0, 0, false, false, {"x"}, "rad2deg", doc_rad2deg);


static double f64_rad2deg(double x) {
  static constexpr double DEGREES_IN_RADIAN = 57.295779513082323;
  return x * DEGREES_IN_RADIAN;
}

static float f32_rad2deg(float x) {
  static constexpr float DEGREES_IN_RADIAN = 57.2957802f;
  return x * DEGREES_IN_RADIAN;
}

umaker_ptr resolve_op_rad2deg(SType stype) {
  return _resolve_trig(stype, "rad2deg", f32_rad2deg, f64_rad2deg);
}




}}  // namespace dt::expr
