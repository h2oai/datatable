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


//------------------------------------------------------------------------------
// Op::ARCTAN2
//------------------------------------------------------------------------------

static const char* doc_arctan2 =
R"(arctan2(x, y)
--

Arc-tangent of y/x, taking into account the signs of x and y.

This function returns the measure of the angle between the ray O(x,y)
and the horizontal abscissae Ox. When both x and y are zero, the
return value is zero.)";

static
py::PKArgs args_arctan2(2, 0, 0, false, false, {"x", "y"}, "arctan2", doc_arctan2);




//------------------------------------------------------------------------------
// Op::HYPOT
//------------------------------------------------------------------------------

static const char* doc_hypot =
R"(hypot(x, y)
--

The length of the hypotenuse of a right triangle with sides x and y.
Equivalent to ``sqrt(x*x + y*y)``.
)";

static
py::PKArgs args_hypot(2, 0, 0, false, false, {"x", "y"}, "hypot", doc_hypot);




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
// Op::COPYSIGN
//------------------------------------------------------------------------------

static const char* doc_copysign =
R"(copysign(x, y)
--

Return a float with the magnitude of x and the sign of y.
)";

static
py::PKArgs args_copysign(2, 0, 0, false, false, {"x", "y"}, "copysign",
                         doc_copysign);




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
