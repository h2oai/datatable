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
  Outputs outputs(wf);
  for (const Expr& arg : inputs) {
    outputs.append( arg.evaluate(wf) );
  }
  return outputs;
}


Outputs Head_List::evaluate_f(workframe&, size_t) const {
  throw TypeError() << "A list or a sequence cannot be used as an f-selector";
}



static const char* _name_type(Head::Kind t) {
  switch (t) {
    case Head::Kind::Unknown:
    case Head::Kind::None:     return "None";
    case Head::Kind::Bool:     return "bool";
    case Head::Kind::Int:      return "integer";
    case Head::Kind::Str:      return "string";
    case Head::Kind::Func:     return "expression";
    case Head::Kind::Type:     return "type";
    case Head::Kind::SliceAll: return "slice";
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
static Head::Kind _resolve_list_kind(const vecExpr& inputs) {
  auto listkind = Head::Kind::Unknown;
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto kind = inputs[i].get_expr_kind();
    xassert(kind != Head::Kind::Unknown);
    if (kind == listkind) continue;
    if (kind != Head::Kind::Bool && listkind != Head::Kind::Bool) {
      if (kind == Head::Kind::None) continue;
      if (kind == Head::Kind::SliceAll &&
          (listkind == Head::Kind::Int ||
           listkind == Head::Kind::Str)) continue;
      if (kind == Head::Kind::Frame)    kind = Head::Kind::Func;
      if (kind == Head::Kind::SliceInt) kind = Head::Kind::Int;
      if (kind == Head::Kind::SliceStr) kind = Head::Kind::Str;
      if (kind == Head::Kind::Float) {
        throw TypeError()
          << "A floating value cannot be used as a column selector";
      }
      if (kind == Head::Kind::List) {
          throw TypeError()
            << "Nested lists are not supported as a column selector";
      }
      if (listkind == Head::Kind::Unknown) listkind = kind;
      if (listkind == Head::Kind::SliceAll &&
          (kind == Head::Kind::Int || kind == Head::Kind::Str)) listkind = kind;
      if (kind == listkind) continue;
    }
    throw TypeError() << "Mixed selector types are not allowed. Element "
        << i << " is of type " << _name_type(kind) << ", whereas the "
        "previous element(s) were of type " << _name_type(listkind);
  }
  if (listkind == Head::Kind::Unknown ||
      listkind == Head::Kind::SliceAll) return Head::Kind::Int;
  return listkind;
}


static Outputs _evaluate_bool_list(const vecExpr& inputs, workframe& wf) {
  DataTable* df = wf.get_datatable(0);
  if (inputs.size() != df->ncols) {
    throw ValueError()
        << "The length of boolean list in j selector does not match the "
           "number of columns in the Frame: "
        << inputs.size() << " vs " << df->ncols;
  }
  Outputs outputs(wf);
  for (size_t i = 0; i < inputs.size(); ++i) {
    bool x = inputs[i].evaluate_as_bool();
    if (x) outputs.add_column(0, i);
  }
  return outputs;
}


static Outputs _evaluate_f_list(const vecExpr& inputs, workframe& wf) {
  Outputs outputs(wf);
  for (const Expr& arg : inputs) {
    outputs.append( arg.evaluate_f(wf, 0) );
  }
  return outputs;
}


Outputs Head_List::evaluate_j(const vecExpr& inputs, workframe& wf) const
{
  auto kind = _resolve_list_kind(inputs);
  if (kind == Kind::Bool) return _evaluate_bool_list(inputs, wf);
  if (kind == Kind::Func) return evaluate(inputs, wf);
  return _evaluate_f_list(inputs, wf);
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
  Outputs outputs(wf);
  for (size_t i = 0; i < inputs.size(); ++i) {
    Outputs arg_out = inputs[i].evaluate(wf);
    arg_out.apply_name(names[i]);
    outputs.append( std::move(arg_out) );
  }
  return outputs;
}


Outputs Head_NamedList::evaluate_f(workframe&, size_t) const {
  throw TypeError() << "A dictionary cannot be used as an f-selector";
}


Outputs Head_NamedList::evaluate_j(const vecExpr& inputs, workframe& wf) const {
  return evaluate(inputs, wf);
}




}}  // namespace dt::expr
