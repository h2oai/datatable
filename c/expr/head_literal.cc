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
#include "column/column_const.h"
#include "expr/head_literal.h"
#include "expr/expr.h"
#include "expr/outputs.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Head_Literal
//------------------------------------------------------------------------------

Outputs Head_Literal::evaluate(const vecExpr& inputs, workframe&) const {
  (void) inputs;
  xassert(inputs.size() == 0);
  return Outputs().add(eval_as_literal(), Outputs::GroupToOne);
}



Outputs Head_Literal::evaluate_j(const vecExpr& inputs, workframe& wf) const {
  (void) inputs;
  xassert(inputs.size() == 0);
  return eval_as_selector(wf, 0);
}



Outputs Head_Literal::evaluate_f(
    const vecExpr& inputs, workframe& wf, size_t frame_id) const
{
  (void) inputs;
  xassert(inputs.size() == 0);
  return eval_as_selector(wf, frame_id);
}




}}  // namespace dt::expr
