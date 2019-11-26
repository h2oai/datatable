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
#include "expr/fbinary/bimaker.h"
#include "expr/fbinary/bimaker_impl.h"
#include "python/args.h"
namespace dt {
namespace expr {


static SType _resolve_math_stypes(SType stype1, SType stype2,
                                  SType* uptype1, SType* uptype2)
{
  SType stype0 = common_stype(stype1, stype2);
  if (stype0 == SType::BOOL || ::info(stype0).ltype() == LType::INT) {
    stype0 = SType::FLOAT64;
  }
  *uptype1 = (stype0 == stype1)? SType::VOID : stype0;
  *uptype2 = (stype0 == stype2)? SType::VOID : stype0;
  return stype0;
}



//------------------------------------------------------------------------------
// Op::ARCTAN2
//------------------------------------------------------------------------------

static const char* doc_atan2 =
R"(atan2(x, y)
--

Arc-tangent of y/x, taking into account the signs of x and y.

This function returns the angle between the ray O(x,y) and the
horizontal abscissa Ox. When both x and y are zero, the return value
is zero.
)";

py::PKArgs args_atan2(2, 0, 0, false, false, {"x", "y"}, "atan2", doc_atan2);


template <typename T>
static bimaker_ptr _atan2(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::atan2, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_atan2(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);

  switch (stype0) {
    case SType::FLOAT32:  return _atan2<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _atan2<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `atan2()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::HYPOT
//------------------------------------------------------------------------------

static const char* doc_hypot =
R"(hypot(x, y)
--

The length of the hypotenuse of a right triangle with sides x and y.
Equivalent to ``sqrt(x*x + y*y)``.
)";

py::PKArgs args_hypot(2, 0, 0, false, false, {"x", "y"}, "hypot", doc_hypot);


template <typename T>
static bimaker_ptr _hypot(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::hypot, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_hypot(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _hypot<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _hypot<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `hypot()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::POWERFN
//------------------------------------------------------------------------------

static const char* doc_pow =
R"(pow(x, y)
--

Number x raised to the power y. The return value will be float, even
if the arguments x and y are integers.

This function is equivalent to `x ** y`.
)";

py::PKArgs args_pow(2, 0, 0, false, false, {"x", "y"}, "pow", doc_pow);


template <typename T>
static bimaker_ptr _pow(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::pow, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_pow(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _pow<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _pow<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `pow()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::COPYSIGN
//------------------------------------------------------------------------------

static const char* doc_copysign =
R"(copysign(x, y)
--

Return a float with the magnitude of x and the sign of y.
)";

py::PKArgs args_copysign(2, 0, 0, false, false, {"x", "y"}, "copysign",
                         doc_copysign);


template <typename T>
static bimaker_ptr _copysign(SType uptype1, SType uptype2, SType outtype) {
  return bimaker1<T, T, T>::make(std::copysign, uptype1, uptype2, outtype);
}


bimaker_ptr resolve_fn_copysign(SType stype1, SType stype2) {
  SType uptype1, uptype2;
  SType stype0 = _resolve_math_stypes(stype1, stype2, &uptype1, &uptype2);
  switch (stype0) {
    case SType::FLOAT32:  return _copysign<float>(uptype1, uptype2, stype0);
    case SType::FLOAT64:  return _copysign<double>(uptype1, uptype2, stype0);
    default:
      throw TypeError() << "Cannot apply function `copysign()` to columns with "
          "types `" << stype1 << "` and `" << stype2 << "`";
  }
}




//------------------------------------------------------------------------------
// Op::LOGADDEXP
//------------------------------------------------------------------------------

static const char* doc_logaddexp =
R"(logaddexp(x, y)
--

The logarithm of the sum of exponents of x and y. This function is
equivalent to ``log(exp(x) + exp(y))``, but does not suffer from
catastrophic precision loss for small values of x and y.
)";

static
py::PKArgs args_logaddexp(2, 0, 0, false, false, {"x", "y"}, "logaddexp",
                          doc_logaddexp);




//------------------------------------------------------------------------------
// Op::LOGADDEXP2
//------------------------------------------------------------------------------

static const char* doc_logaddexp2 =
R"(logaddexp2(x, y)
--

Binary logarithm of the sum of binary exponents of x and y. This
function is equivalent to ``log2(exp2(x) + exp2(y))``, but does
not suffer from catastrophic precision loss for small values of
x and y.
)";

static
py::PKArgs args_logaddexp2(2, 0, 0, false, false, {"x", "y"}, "logaddexp2",
                           doc_logaddexp2);




//------------------------------------------------------------------------------
// Op::NEXTAFTER
//------------------------------------------------------------------------------

static const char* doc_nextafter =
R"(nextafter(x, y)
--

Returns the next representable value of x in the direction of y.
If any argument is integer (or boolean), it is converted to float64
first.
)";

static
py::PKArgs args_nextafter(2, 0, 0, false, false, {"x", "y"}, "nextafter",
                          doc_nextafter);




}}  // namespace dt::expr
