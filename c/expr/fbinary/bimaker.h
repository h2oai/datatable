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
#include "types.h"
namespace dt {
namespace expr {


/**
  * Main function for computing binary operations between columns.
  */
Column new_binaryop(Op opcode, Column&& col1, Column&& col2);

// OLD, defined in expr_binaryop.cc
Column binaryop(Op opcode, Column& col1, Column& col2);



//------------------------------------------------------------------------------
// bimaker class
//------------------------------------------------------------------------------

class bimaker {
  public:
    virtual ~bimaker();
    virtual Column compute(Column&&, Column&&) const = 0;
};

using bimaker_ptr = std::unique_ptr<bimaker>;



//------------------------------------------------------------------------------
// Resolvers
//------------------------------------------------------------------------------

bimaker_ptr resolve_op(Op, SType, SType);
bimaker_ptr resolve_op_eq(SType, SType);
bimaker_ptr resolve_op_ne(SType, SType);
bimaker_ptr resolve_op_relational(Op, SType, SType);




}}  // namespace dt::expr
#endif
