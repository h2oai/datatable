//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "expr/expr_cast.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// expr_cast
//------------------------------------------------------------------------------

expr_cast::expr_cast(pexpr&& a, SType s)
  : arg(std::move(a)),
    stype(s) {}


SType expr_cast::resolve(const workframe& wf) {
  (void) arg->resolve(wf);
  return stype;
}


GroupbyMode expr_cast::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


OColumn expr_cast::evaluate_eager(workframe& wf) {
  auto arg_col = arg->evaluate_eager(wf);
  return OColumn(arg_col->cast(stype));
}


vcolptr expr_cast::evaluate_lazy(workframe& wf) {
  auto arg_vcol = arg->evaluate_lazy(wf);
  return cast(std::move(arg_vcol), stype);
}



}} // namespace dt::expr
