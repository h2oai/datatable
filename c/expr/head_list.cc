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
#include "expr/head_list.h"
#include "expr/expr.h"
#include "expr/outputs.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Head_List
//------------------------------------------------------------------------------

Head::Kind Head_List::get_expr_kind() const {
  return Head::Kind::List;
}

Outputs Head_List::evaluate(const vecExpr& inputs, workframe& wf) const {
  Outputs res;
  for (const Expr& arg : inputs) {
    res.append( arg.evaluate(wf) );
  }
  return res;
}


Outputs Head_List::evaluate_j(const vecExpr& inputs, workframe& wf) const {
  for (const Expr& arg : inputs) {
  }
}




//------------------------------------------------------------------------------
// Head_NamedList
//------------------------------------------------------------------------------

Head_NamedList::Head_NamedList(strvec&& names_)
  : names(std::move(names_)) {}

Head::Kind Head_NamedList::get_expr_kind() const {
  return Head::Kind::List;
}


Outputs Head_NamedList::evaluate(const vecExpr& inputs, workframe& wf) const {
  xassert(inputs.size() == names.size());
  Outputs res;
  for (size_t i = 0; i < inputs.size(); ++i) {
    Outputs arg_out = inputs[i].evaluate(wf);
    arg_out.apply_name(names[i]);
    res.append( std::move(arg_out) );
  }
  return res;
}


Outputs Head_NamedList::evaluate_f(const vecExpr&, workframe&, size_t) const {
  throw TypeError() << "A dictionary cannot be used as an f-selector";
}


Outputs Head_NamedList::evaluate_j(const vecExpr& inputs, workframe& wf) const {
  return evaluate(inputs, wf);
}




}}  // namespace dt::expr
