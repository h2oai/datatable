//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#ifndef dt_EXPR_OP_h
#define dt_EXPR_OP_h
#include <cstddef>    // std::size_t
namespace dt {
namespace expr {
using size_t = std::size_t;


static constexpr size_t UNOP_FIRST    = 101;
static constexpr size_t UNOP_LAST     = 103;
static constexpr size_t BINOP_FIRST   = 201;
static constexpr size_t BINOP_LAST    = 218;
static constexpr size_t REDUCER_FIRST = 401;
static constexpr size_t REDUCER_LAST  = 414;
static constexpr size_t MATH_FIRST    = 501;
static constexpr size_t MATH_LAST     = 554;
static constexpr size_t UNOP_COUNT    = UNOP_LAST - UNOP_FIRST + 1;
static constexpr size_t BINOP_COUNT   = BINOP_LAST - BINOP_FIRST + 1;
static constexpr size_t REDUCER_COUNT = REDUCER_LAST - REDUCER_FIRST + 1;


// The values in this enum must be kept in sync with Python enum OpCodes in
// file "datatable/expr/expr.py"
enum class Op : size_t {
  // Misc
  NOOP = 0,
  CAST = 2,
  SETPLUS = 3,
  SETMINUS = 4,
  SHIFTFN = 5,              // head_func_shift.cc

  // Unary
  UPLUS = UNOP_FIRST,       // funary/basic.cc
  UMINUS,                   // funary/basic.cc
  UINVERT = UNOP_LAST,      // funary/basic.cc

  // Binary
  AND = 208,                // fbinary/bitwise.cc
  XOR = 209,                // fbinary/bitwise.cc
  OR = 210,                 // fbinary/bitwise.cc
  LSHIFT = 211,             // fbinary/bitwise.cc
  RSHIFT = 212,             // fbinary/bitwise.cc

  // Reducers
  MEAN = REDUCER_FIRST,     // head_reduce_unary.cc
  MIN,                      // head_reduce_unary.cc
  MAX,                      // head_reduce_unary.cc
  STDEV,                    // head_reduce_unary.cc
  FIRST,                    // head_reduce_unary.cc
  LAST,                     // head_reduce_unary.cc
  SUM,                      // head_reduce_unary.cc
  COUNT,                    // head_reduce_unary.cc
  COUNT0,                   // head_reduce_nullary.cc
  MEDIAN,                   // head_reduce_unary.cc
  COV,                      // head_reduce_binary.cc
  CORR,                     // head_reduce_binary.cc
  COUNTNA,                  // head_reduce_unary.cc
  NUNIQUE = REDUCER_LAST,   // head_reduce_unary.cc


  // Math: trigonometric
  SIN = MATH_FIRST,         // funary/trigonometric.cc
  COS,                      // funary/trigonometric.cc
  TAN,                      // funary/trigonometric.cc
  ARCSIN,                   // funary/trigonometric.cc
  ARCCOS,                   // funary/trigonometric.cc
  ARCTAN,                   // funary/trigonometric.cc
  ARCTAN2,                  // fbinary/math.cc
  HYPOT,                    // fbinary/math.cc
  DEG2RAD,                  // funary/trigonometric.cc
  RAD2DEG,                  // funary/trigonometric.cc

  // Math: exponential/power
  CBRT,                     // funary/exponential.cc
  EXP,                      // funary/exponential.cc
  EXP2,                     // funary/exponential.cc
  EXPM1,                    // funary/exponential.cc
  LOG,                      // funary/exponential.cc
  LOG10,                    // funary/exponential.cc
  LOG1P,                    // funary/exponential.cc
  LOG2,                     // funary/exponential.cc
  LOGADDEXP,                // fbinary/math.cc
  LOGADDEXP2,               // fbinary/math.cc
  POWERFN,                  // fbinary/math.cc
  SQRT,                     // funary/exponential.cc
  SQUARE,                   // funary/exponential.cc

  // Math: special
  ERF,                      // funary/special.cc
  ERFC,                     // funary/special.cc
  GAMMA,                    // funary/special.cc
  LGAMMA,                   // funary/special.cc

  // Math: floating-point
  ABS,                      // funary/floating.cc
  CEIL,                     // funary/floating.cc
  COPYSIGN,                 // fbinary/math.cc
  FABS,                     // funary/floating.cc
  FLOOR,                    // funary/floating.cc
  FREXP,
  ISCLOSE,                  // head_func_isclose.cc
  ISFINITE,                 // funary/floating.cc
  ISINF,                    // funary/floating.cc
  ISNA,                     // funary/floating.cc
  LDEXP,
  MODF,
  RINT,                     // fumary/floating.cc
  SIGN,                     // funary/floating.cc
  SIGNBIT,                  // funary/floating.cc
  TRUNC,                    // funary/floating.cc

  // Math: misc
  CLIP,
  DIVMOD,
  FMOD,
  MAXIMUM,
  MINIMUM = MATH_LAST,
};



}}  // namespace dt::expr
#endif
