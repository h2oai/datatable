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


//------------------------------------------------------------------------------
// Op::SIN/COS/TAN/ARCSIN/ARCCOS/ARCTAN
//------------------------------------------------------------------------------

py::PKArgs args_arccos(
  1, 0, 0, false, false, {"x"}, "arccos",
R"(Inverse trigonometric cosine of x.

The returned value is in the interval [0, pi], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
cos() in the sense that `cos(arccos(x)) == x`.
)");


py::PKArgs args_arcsin(
  1, 0, 0, false, false, {"x"}, "arcsin",
R"(Inverse trigonometric sine of x.

The returned value is in the interval [-pi/2, pi/2], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
sin() in the sense that `sin(arcsin(x)) == x`.
)");


py::PKArgs args_arctan(
  1, 0, 0, false, false, {"x"}, "arctan",
R"(Inverse trigonometric tangent of x.)");


py::PKArgs args_cos(
  1, 0, 0, false, false, {"x"}, "cos", "Trigonometric cosine of x.");


py::PKArgs args_sin(
  1, 0, 0, false, false, {"x"}, "sin", "Trigonometric sine of x.");


py::PKArgs args_tan(
  1, 0, 0, false, false, {"x"}, "tan", "Trigonometric tangent of x.");



/**
  * All standard trigonometric functions have the same signature:
  * all numeric types map into FLOAT64, except for FLOAT32 which maps
  * into itself.
  */
static umaker_ptr _resolve_trig(SType stype, const char* name,
                                func32_t fn32, func64_t fn64)
{
  if (stype == SType::VOID) return umaker_ptr(new umaker_copy());
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


umaker_ptr resolve_op_sin(SType stype) {
  return _resolve_trig(stype, "sin", &std::sin, &std::sin);
}

umaker_ptr resolve_op_cos(SType stype) {
  return _resolve_trig(stype, "cos", &std::cos, &std::cos);
}

umaker_ptr resolve_op_tan(SType stype) {
  return _resolve_trig(stype, "tan", &std::tan, &std::tan);
}

umaker_ptr resolve_op_arcsin(SType stype) {
  return _resolve_trig(stype, "arcsin", &std::asin, &std::asin);
}

umaker_ptr resolve_op_arccos(SType stype) {
  return _resolve_trig(stype, "arccos", &std::acos, &std::acos);
}

umaker_ptr resolve_op_arctan(SType stype) {
  return _resolve_trig(stype, "arctan", &std::atan, &std::atan);
}




//------------------------------------------------------------------------------
// Op::DEG2RAD
//------------------------------------------------------------------------------

py::PKArgs args_deg2rad(
  1, 0, 0, false, false, {"x"}, "deg2rad",
  "Convert angle measured in degrees into radians.");


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

py::PKArgs args_rad2deg(
  1, 0, 0, false, false, {"x"}, "rad2deg",
  "Convert angle measured in radians into degrees.");


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
