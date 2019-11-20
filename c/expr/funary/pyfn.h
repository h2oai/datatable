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
#ifndef dt_EXPR_FUNARY_PYFN_h
#define dt_EXPR_FUNARY_PYFN_h
#include "python/args.h"
namespace dt {
namespace expr {


// Trigonometric
extern py::PKArgs args_sin;
extern py::PKArgs args_cos;
extern py::PKArgs args_tan;
extern py::PKArgs args_arcsin;
extern py::PKArgs args_arccos;
extern py::PKArgs args_arctan;
extern py::PKArgs args_deg2rad;
extern py::PKArgs args_rad2deg;

// Hyperbolic
extern py::PKArgs args_sinh;
extern py::PKArgs args_cosh;
extern py::PKArgs args_tanh;
extern py::PKArgs args_arsinh;
extern py::PKArgs args_arcosh;
extern py::PKArgs args_artanh;

// Exponential/power
extern py::PKArgs args_cbrt;
extern py::PKArgs args_exp;
extern py::PKArgs args_exp2;
extern py::PKArgs args_expm1;
extern py::PKArgs args_log;
extern py::PKArgs args_log10;
extern py::PKArgs args_log1p;
extern py::PKArgs args_log2;
extern py::PKArgs args_sqrt;
extern py::PKArgs args_square;




}}  // namespace dt::expr
#endif
