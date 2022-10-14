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
#include "expr/eval_context.h"
#include "expr/fexpr_dict.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_Dict::FExpr_Dict(strvec&& names, vecExpr&& args)
  : names_(std::move(names)),
    args_(std::move(args))
{
  xassert(names_.size() == args_.size());
}


ptrExpr FExpr_Dict::make(py::robj src) {
  strvec names;
  vecExpr args;
  for (auto kv : src.to_pydict()) {
    if (!kv.first.is_string()) {
      throw TypeError() << "Keys in the dictionary must be strings";
    }
    names.push_back(kv.first.to_string());
    args.emplace_back(as_fexpr(kv.second));
  }
  return ptrExpr(new FExpr_Dict(std::move(names), std::move(args)));
}




//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_Dict::evaluate_n(EvalContext& ctx) const {
  Workframe outputs(ctx);
  for (size_t i = 0; i < args_.size(); ++i) {
    Workframe arg_out = args_[i]->evaluate_n(ctx);
    arg_out.rename(names_[i]);
    outputs.cbind( std::move(arg_out) );
  }

  outputs.sync_gtofew_columns();
  return outputs;
}


Workframe FExpr_Dict::evaluate_r(EvalContext& ctx, const sztvec&) const {
  return evaluate_n(ctx);
}


Workframe FExpr_Dict::evaluate_f(EvalContext&, size_t) const {
  throw TypeError() << "A dictionary cannot be used as an f-selector";
}


Workframe FExpr_Dict::evaluate_j(EvalContext& ctx) const {
  return evaluate_n(ctx);
}


RowIndex FExpr_Dict::evaluate_i(EvalContext&) const {
  throw TypeError() << "A dictionary cannot be used as an i-selector";
}


RiGb FExpr_Dict::evaluate_iby(EvalContext&) const {
  throw TypeError() << "A dictionary cannot be used as an i-selector";
}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

Kind FExpr_Dict::get_expr_kind() const {
  return Kind::Dict;
}


int FExpr_Dict::precedence() const noexcept {
  return 17;
}


std::string FExpr_Dict::repr() const {
  std::string out = "{";
  for (size_t i = 0; i < names_.size(); ++i) {
    if (i) out += ", ";
    out += names_[i];  // TODO: escape if necessary
    out += '=';
    out += args_[i]->repr();
  }
  out += '}';
  return out;
}



}}  // namespace dt::expr
