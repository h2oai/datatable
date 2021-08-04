//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "documentation.h"
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

py::PKArgs args_erf(1, 0, 0, false, false, {"x"}, "erf", dt::doc_math_erf);

umaker_ptr resolve_op_erf(SType stype) {
  return _resolve_special(stype, "erf", &std::erf, &std::erf);
}




//------------------------------------------------------------------------------
// Op::ERFC
//------------------------------------------------------------------------------

py::PKArgs args_erfc(1, 0, 0, false, false, {"x"}, "erfc", dt::doc_math_erfc);

umaker_ptr resolve_op_erfc(SType stype) {
  return _resolve_special(stype, "erfc", &std::erfc, &std::erfc);
}




//------------------------------------------------------------------------------
// Op::GAMMA
//------------------------------------------------------------------------------

py::PKArgs args_gamma(1, 0, 0, false, false, {"x"}, "gamma", dt::doc_math_gamma);

umaker_ptr resolve_op_gamma(SType stype) {
  return _resolve_special(stype, "gamma", &std::tgamma, &std::tgamma);
}




//------------------------------------------------------------------------------
// Op::LGAMMA
//------------------------------------------------------------------------------

py::PKArgs args_lgamma(1, 0, 0, false, false, {"x"}, "lgamma", dt::doc_math_lgamma);

umaker_ptr resolve_op_lgamma(SType stype) {
  return _resolve_special(stype, "lgamma", &std::lgamma, &std::lgamma);
}




}}  // namespace dt::expr
