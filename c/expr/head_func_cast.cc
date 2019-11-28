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
#include "expr/expr.h"
#include "expr/head_func.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


Head_Func_Cast::Head_Func_Cast(SType s) : stype(s) {}


Workframe Head_Func_Cast::evaluate_n(
    const vecExpr& args, EvalContext& ctx, bool) const
{
  xassert(args.size() == 1);
  Workframe outputs = args[0].evaluate_n(ctx);
  size_t n = outputs.ncols();
  for (size_t i = 0; i < n; ++i) {
    Column col = outputs.retrieve_column(i);
    col.cast_inplace(stype);
    outputs.replace_column(i, std::move(col));
  }
  return outputs;
}




}}  // namespace dt::expr
