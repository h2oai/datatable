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
#ifndef dt_EXPR_FBINARY_BIMAKER_h
#define dt_EXPR_FBINARY_BIMAKER_h
#include <memory>
#include "expr/declarations.h"
#include "expr/op.h"
#include "python/args.h"
#include "types.h"
namespace dt {
namespace expr {


/**
  * Main method for computing binary operators between columns.
  *
  * The signature of this function is that it takes an opcode (one of
  * BINOP_FIRST..BINOP_LAST) and a pair of columns, and returns a new
  * virtual column that is the result of application of the op to the
  * given columns.
  *
  * ----------
  * Internally, this method relies on a collection of `bimaker`
  * singleton objects. Each such object implements "binaryop"
  * functionality for a specific opcode and specific stypes of `col1`
  * and `col2`.
  *
  * Thus, this method works in 2 steps: (1) find the `bimaker` object
  * corresponding to the given opcode and the stypes of both columns,
  * and (2) invoke `.compute()` method on that object to produce the
  * result. The first step also has 2 levels: first we look up the
  * bimaker object in the memoized dictionary of all bimaker objects
  * seen so far, or otherwise resolve the bimaker object using a
  * network of `resolve_op_*()` methods (storing the resolved object
  * in the memoized dictionary for later use).
  */
Column binaryop(Op opcode, Column&& col1, Column&& col2);



//------------------------------------------------------------------------------
// bimaker class [private]
//------------------------------------------------------------------------------

class bimaker {
  public:
    virtual ~bimaker();
    virtual Column compute(Column&&, Column&&) const = 0;
};

using bimaker_ptr = std::unique_ptr<bimaker>;



//------------------------------------------------------------------------------
// Resolvers [private]
//------------------------------------------------------------------------------

// Main resolver, calls individual-op resolvers below
bimaker_ptr resolve_op(Op, SType, SType);

bimaker_ptr resolve_op_plus(SType, SType);
bimaker_ptr resolve_op_minus(SType, SType);
bimaker_ptr resolve_op_multiply(SType, SType);
bimaker_ptr resolve_op_divide(SType, SType);
bimaker_ptr resolve_op_intdiv(SType, SType);
bimaker_ptr resolve_op_modulo(SType, SType);
bimaker_ptr resolve_op_power(SType, SType);
bimaker_ptr resolve_op_and(SType, SType);
bimaker_ptr resolve_op_or(SType, SType);
bimaker_ptr resolve_op_xor(SType, SType);
bimaker_ptr resolve_op_lshift(SType, SType);
bimaker_ptr resolve_op_rshift(SType, SType);
bimaker_ptr resolve_op_eq(SType, SType);
bimaker_ptr resolve_op_ne(SType, SType);
bimaker_ptr resolve_op_lt(SType, SType);
bimaker_ptr resolve_op_gt(SType, SType);
bimaker_ptr resolve_op_le(SType, SType);
bimaker_ptr resolve_op_ge(SType, SType);

bimaker_ptr resolve_fn_atan2(SType, SType);
bimaker_ptr resolve_fn_hypot(SType, SType);
bimaker_ptr resolve_fn_pow(SType, SType);
bimaker_ptr resolve_fn_copysign(SType, SType);
bimaker_ptr resolve_fn_logaddexp(SType, SType);
bimaker_ptr resolve_fn_logaddexp2(SType, SType);
bimaker_ptr resolve_fn_fmod(SType, SType);
bimaker_ptr resolve_fn_ldexp(SType, SType);



//------------------------------------------------------------------------------
// Python interface
//------------------------------------------------------------------------------

extern py::PKArgs args_atan2;
extern py::PKArgs args_hypot;
extern py::PKArgs args_pow;
extern py::PKArgs args_copysign;
extern py::PKArgs args_logaddexp;
extern py::PKArgs args_logaddexp2;
extern py::PKArgs args_fmod;
extern py::PKArgs args_ldexp;




}}  // namespace dt::expr
#endif
