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

py::PKArgs args_sinh(1, 0, 0, false, false, {"x"}, "sinh", dt::doc_math_sinh);


umaker_ptr resolve_op_sinh(SType stype) {
  return _resolve_hyp(stype, "sinh", &std::sinh, &std::sinh);
}




//------------------------------------------------------------------------------
// Op::COSH
//------------------------------------------------------------------------------

py::PKArgs args_cosh(1, 0, 0, false, false, {"x"}, "cosh", dt::doc_math_cosh);


umaker_ptr resolve_op_cosh(SType stype) {
  return _resolve_hyp(stype, "cosh", &std::cosh, &std::cosh);
}




//------------------------------------------------------------------------------
// Op::TANH
//------------------------------------------------------------------------------

py::PKArgs args_tanh(1, 0, 0, false, false, {"x"}, "tanh", dt::doc_math_tanh);


umaker_ptr resolve_op_tanh(SType stype) {
  return _resolve_hyp(stype, "tanh", &std::tanh, &std::tanh);
}




//------------------------------------------------------------------------------
// Op::ARSINH
//------------------------------------------------------------------------------

py::PKArgs args_arsinh(1, 0, 0, false, false, {"x"}, "arsinh", dt::doc_math_arsinh);


umaker_ptr resolve_op_arsinh(SType stype) {
  return _resolve_hyp(stype, "arsinh", &std::asinh, &std::asinh);
}




//------------------------------------------------------------------------------
// Op::ARCOSH
//------------------------------------------------------------------------------

py::PKArgs args_arcosh(1, 0, 0, false, false, {"x"}, "arcosh", dt::doc_math_arcosh);


umaker_ptr resolve_op_arcosh(SType stype) {
  return _resolve_hyp(stype, "arcosh", &std::acosh, &std::acosh);
}




//------------------------------------------------------------------------------
// Op::ARTANH
//------------------------------------------------------------------------------

py::PKArgs args_artanh(1, 0, 0, false, false, {"x"}, "artanh", dt::doc_math_artanh);


umaker_ptr resolve_op_artanh(SType stype) {
  return _resolve_hyp(stype, "artanh", &std::atanh, &std::atanh);
}




}}  // namespace dt::expr
