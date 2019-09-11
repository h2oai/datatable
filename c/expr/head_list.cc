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
#include "expr/workframe.h"
#include "expr/eval_context.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Head_List
//------------------------------------------------------------------------------

Kind Head_List::get_expr_kind() const {
  return Kind::List;
}

Workframe Head_List::evaluate_n(const vecExpr& inputs, EvalContext& ctx) const {
  Workframe outputs(ctx);
  for (const Expr& arg : inputs) {
    outputs.cbind( arg.evaluate_n(ctx) );
  }
  return outputs;
}


Workframe Head_List::evaluate_f(EvalContext&, size_t) const {
  throw TypeError() << "A list or a sequence cannot be used as an f-selector";
}



static const char* _name_type(Kind t) {
  switch (t) {
    case Kind::Unknown:
    case Kind::None:     return "None";
    case Kind::Bool:     return "bool";
    case Kind::Int:      return "integer";
    case Kind::Str:      return "string";
    case Kind::Func:     return "expression";
    case Kind::Type:     return "type";
    case Kind::SliceAll: return "slice";
    default:             return "?";
  }
}

// Resolve logic should work as follows:
//  - List[bool] -> Kind::Bool
//  - List[int | slice_int | slice_all | None] -> Kind::Int
//  - List[str | slice_str | slice_all | None] -> Kind::Str
//  - List[frame | func | None] -> Kind::Func
//  - List[type | None] -> Kind::Type
//
// If a list contains either floats or lists, or invalid combination of
// element kinds, an error must be thrown.
// A list containing only `None` or `slice_all` should resolve as Kind::Int.
//
static Kind _resolve_list_kind(const vecExpr& inputs) {
  auto listkind = Kind::Unknown;
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto kind = inputs[i].get_expr_kind();
    xassert(kind != Kind::Unknown);
    if (kind == listkind) continue;
    if (kind != Kind::Bool && listkind != Kind::Bool) {
      if (kind == Kind::None) continue;
      if (kind == Kind::SliceAll &&
          (listkind == Kind::Int ||
           listkind == Kind::Str)) continue;
      if (kind == Kind::Frame)    kind = Kind::Func;
      if (kind == Kind::SliceInt) kind = Kind::Int;
      if (kind == Kind::SliceStr) kind = Kind::Str;
      if (kind == Kind::Float) {
        throw TypeError()
          << "A floating value cannot be used as a column selector";
      }
      if (kind == Kind::List) {
          throw TypeError()
            << "Nested lists are not supported as a column selector";
      }
      if (listkind == Kind::Unknown) listkind = kind;
      if (listkind == Kind::SliceAll &&
          (kind == Kind::Int || kind == Kind::Str)) listkind = kind;
      if (kind == listkind) continue;
    }
    if (kind == Kind::Bool && listkind == Kind::Unknown) {
      listkind = Kind::Bool;
      continue;
    }
    throw TypeError() << "Mixed selector types are not allowed. Element "
        << i << " is of type " << _name_type(kind) << ", whereas the "
        "previous element(s) were of type " << _name_type(listkind);
  }
  if (listkind == Kind::Unknown ||
      listkind == Kind::SliceAll) return Kind::Int;
  return listkind;
}


static Workframe _evaluate_bool_list(const vecExpr& inputs, EvalContext& ctx) {
  DataTable* df = ctx.get_datatable(0);
  if (inputs.size() != df->ncols) {
    throw ValueError()
        << "The length of boolean list in j selector does not match the "
           "number of columns in the Frame: "
        << inputs.size() << " vs " << df->ncols;
  }
  Workframe outputs(ctx);
  for (size_t i = 0; i < inputs.size(); ++i) {
    bool x = inputs[i].evaluate_bool();
    if (x) outputs.add_ref_column(0, i);
  }
  return outputs;
}


static Workframe _evaluate_f_list(const vecExpr& inputs, EvalContext& ctx) {
  Workframe outputs(ctx);
  for (const Expr& arg : inputs) {
    outputs.cbind( arg.evaluate_f(ctx, 0) );
  }
  return outputs;
}


Workframe Head_List::evaluate_j(const vecExpr& inputs, EvalContext& ctx) const
{
  auto kind = _resolve_list_kind(inputs);
  if (kind == Kind::Bool) return _evaluate_bool_list(inputs, ctx);
  if (kind == Kind::Func) return evaluate_n(inputs, ctx);
  return _evaluate_f_list(inputs, ctx);
}




//------------------------------------------------------------------------------
// Head_NamedList
//------------------------------------------------------------------------------

Head_NamedList::Head_NamedList(strvec&& names_)
  : names(std::move(names_)) {}

Kind Head_NamedList::get_expr_kind() const {
  return Kind::List;
}


Workframe Head_NamedList::evaluate_n(const vecExpr& inputs, EvalContext& ctx) const {
  xassert(inputs.size() == names.size());
  Workframe outputs(ctx);
  for (size_t i = 0; i < inputs.size(); ++i) {
    Workframe arg_out = inputs[i].evaluate_n(ctx);
    arg_out.rename(names[i]);
    outputs.cbind( std::move(arg_out) );
  }
  return outputs;
}


Workframe Head_NamedList::evaluate_f(EvalContext&, size_t) const {
  throw TypeError() << "A dictionary cannot be used as an f-selector";
}


Workframe Head_NamedList::evaluate_j(const vecExpr& inputs, EvalContext& ctx) const {
  return evaluate_n(inputs, ctx);
}




}}  // namespace dt::expr
