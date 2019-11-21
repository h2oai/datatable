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
#ifndef dt_EXPR_FUNARY_UMAKER_h
#define dt_EXPR_FUNARY_UMAKER_h
#include <memory>
#include "expr/declarations.h"
#include "expr/op.h"
#include "types.h"
namespace dt {
namespace expr {


/**
  * Main method for computing unary operation on a column.
  *
  * The signature of this function is that it takes a "unary" opcode
  * and a column, and returns a new virtual column that is the result
  * of applying that operation to the given column.
  *
  * ----------
  * Internally, this method relies on a collection of `umaker`
  * singleton objects. Each such object implements "unaryop"
  * functionality for a specific opcode and a specific stype of `col`.
  *
  * Thus, this method works in 2 steps: (1) find the `umaker` object
  * corresponding to the given opcode and the stype of the column,
  * and (2) invoke `.compute()` method on that object to produce the
  * result. The first step also has 2 levels: first we look up the
  * umaker object in the memoized dictionary of all umaker objects
  * seen so far, or otherwise resolve the umaker object using a
  * network of `resolve_op_*()` methods (storing the resolved object
  * in the memoized dictionary for later use).
  */
Column unaryop(Op opcode, Column&& col);

py::oobj unaryop(Op opcode, std::nullptr_t);
py::oobj unaryop(Op opcode, bool value);
py::oobj unaryop(Op opcode, int64_t value);
py::oobj unaryop(Op opcode, double value);
py::oobj unaryop(Op opcode, CString value);



//------------------------------------------------------------------------------
// umaker class [private]
//------------------------------------------------------------------------------

class umaker {
  public:
    virtual ~umaker();
    virtual Column compute(Column&&) const = 0;
};

using umaker_ptr = std::unique_ptr<umaker>;




//------------------------------------------------------------------------------
// Resolvers [private]
//------------------------------------------------------------------------------

// Main resolver, calls individual-op resolvers below
umaker_ptr resolve_op(Op, SType);

// Basic
umaker_ptr resolve_op_uplus(SType);
umaker_ptr resolve_op_uminus(SType);
umaker_ptr resolve_op_uinvert(SType);
umaker_ptr resolve_op_len(SType);

// Trigonometric
umaker_ptr resolve_op_sin(SType);
umaker_ptr resolve_op_cos(SType);
umaker_ptr resolve_op_tan(SType);
umaker_ptr resolve_op_arcsin(SType);
umaker_ptr resolve_op_arccos(SType);
umaker_ptr resolve_op_arctan(SType);
umaker_ptr resolve_op_deg2rad(SType);
umaker_ptr resolve_op_rad2deg(SType);

// Hyperbolic
umaker_ptr resolve_op_sinh(SType);
umaker_ptr resolve_op_cosh(SType);
umaker_ptr resolve_op_tanh(SType);
umaker_ptr resolve_op_arsinh(SType);
umaker_ptr resolve_op_arcosh(SType);
umaker_ptr resolve_op_artanh(SType);

// Exponential/power
umaker_ptr resolve_op_cbrt(SType);
umaker_ptr resolve_op_exp(SType);
umaker_ptr resolve_op_exp2(SType);
umaker_ptr resolve_op_expm1(SType);
umaker_ptr resolve_op_log(SType);
umaker_ptr resolve_op_log10(SType);
umaker_ptr resolve_op_log1p(SType);
umaker_ptr resolve_op_log2(SType);
umaker_ptr resolve_op_sqrt(SType);
umaker_ptr resolve_op_square(SType);

// Special
umaker_ptr resolve_op_erf(SType);
umaker_ptr resolve_op_erfc(SType);
umaker_ptr resolve_op_gamma(SType);
umaker_ptr resolve_op_lgamma(SType);

// Floating-point
umaker_ptr resolve_op_isfinite(SType);
umaker_ptr resolve_op_isinf(SType);
umaker_ptr resolve_op_isna(SType);
umaker_ptr resolve_op_ceil(SType);
umaker_ptr resolve_op_abs(SType);
umaker_ptr resolve_op_fabs(SType);
umaker_ptr resolve_op_floor(SType);
umaker_ptr resolve_op_rint(SType);
umaker_ptr resolve_op_sign(SType);
umaker_ptr resolve_op_signbit(SType);
umaker_ptr resolve_op_trunc(SType);




}}  // namespace dt::expr
#endif
