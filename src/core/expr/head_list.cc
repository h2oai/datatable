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
#include "expr/head_list.h"
#include "expr/expr.h"
#include "expr/workframe.h"
#include "expr/eval_context.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "sort.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Head_List
//------------------------------------------------------------------------------

Kind Head_List::get_expr_kind() const {
  return Kind::List;
}

Workframe Head_List::evaluate_n(
    const vecExpr& inputs, EvalContext& ctx) const
{
  Workframe outputs(ctx);
  for (const auto& arg : inputs) {
    outputs.cbind( arg->evaluate_n(ctx) );
  }
  return outputs;
}


// Evaluate list as a replacement target when replacing columns at
// `indices` within the "root" Frame.
//
Workframe Head_List::evaluate_r(
    const vecExpr& inputs, EvalContext& ctx, const sztvec& indices) const
{
  Workframe outputs(ctx);
  if (inputs.size() == indices.size()) {
    for (size_t i = 0; i < inputs.size(); ++i) {
      outputs.cbind( inputs[i]->evaluate_r(ctx, {indices[i]}) );
    }
  }
  else if (inputs.size() == 1) {
    for (size_t i = 0; i < indices.size(); ++i) {
      outputs.cbind( inputs[0]->evaluate_r(ctx, {indices[i]}) );
    }
  }
  else {
    throw ValueError() << "The LHS of the replacement has " << indices.size()
        << " columns, while the RHS has " << inputs.size()
        << " replacement expressions";
  }
  return outputs;
}



Workframe Head_List::evaluate_f(EvalContext&, size_t) const {
  throw TypeError()
      << "A list or a sequence cannot be used inside an f-selector";
}



static const char* _name_type(Kind t) {
  switch (t) {
    case Kind::Unknown:
    case Kind::None:     return "None";
    case Kind::Bool:     return "bool";
    case Kind::Int:      return "integer";
    case Kind::Float:    return "float";
    case Kind::Str:      return "string";
    case Kind::Func:     return "expression";
    case Kind::Type:     return "type";
    case Kind::SliceAll: return "slice";
    case Kind::SliceInt: return "integer slice";
    case Kind::SliceStr: return "string-slice";
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
    auto kind = inputs[i]->get_expr_kind();
    xassert(kind != Kind::Unknown);
    if (kind == listkind) continue;
    if (kind == Kind::Bool) {
      if (listkind != Kind::Unknown) goto error;
      listkind = Kind::Bool;
      continue;
    }
    if (listkind == Kind::Bool) goto error;
    if (kind == Kind::None) continue;
    if (kind == Kind::SliceAll &&
        (listkind == Kind::Int || listkind == Kind::Str)) continue;
    if (kind == Kind::Frame)    kind = Kind::Func;
    if (kind == Kind::SliceInt) kind = Kind::Int;
    if (kind == Kind::SliceStr) kind = Kind::Str;
    if (kind == Kind::Float) {
      throw TypeError()
        << "A floating value cannot be used as a column selector";
    }
    if (kind == Kind::List || kind == Kind::Dict) {
      throw TypeError()
        << "Nested lists are not supported as a column selector";
    }
    if (listkind == Kind::Unknown) listkind = kind;
    if (listkind == Kind::SliceAll &&
        (kind == Kind::Int || kind == Kind::Str)) listkind = kind;
    if (kind == listkind) continue;

    error:
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
  if (inputs.size() != df->ncols()) {
    throw ValueError()
        << "The length of boolean list in `j` selector does not match the "
           "number of columns in the Frame: "
        << inputs.size() << " vs " << df->ncols();
  }
  Workframe outputs(ctx);
  for (size_t i = 0; i < inputs.size(); ++i) {
    bool x = inputs[i]->evaluate_bool();
    if (x) outputs.add_ref_column(0, i);
  }
  return outputs;
}


static Workframe _evaluate_f_list(
    const vecExpr& inputs, EvalContext& ctx)
{
  Workframe outputs(ctx);
  for (const auto& arg : inputs) {
    outputs.cbind( arg->evaluate_j(ctx) );
  }
  return outputs;
}


Workframe Head_List::evaluate_j(
    const vecExpr& inputs, EvalContext& ctx) const
{
  auto kind = _resolve_list_kind(inputs);
  if (kind == Kind::Bool) return _evaluate_bool_list(inputs, ctx);
  if (kind == Kind::Func) return evaluate_n(inputs, ctx);
  return _evaluate_f_list(inputs, ctx);
}



//------------------------------------------------------------------------------
// i-evaluation
//------------------------------------------------------------------------------

static RowIndex _evaluate_i_other(const vecExpr& inputs, EvalContext& ctx) {
  std::vector<RowIndex> rowindices;
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto ikind = inputs[i]->get_expr_kind();
    if (ikind == Kind::None) continue;
    if (!(ikind == Kind::Int || ikind == Kind::SliceInt ||
          ikind == Kind::SliceAll || ikind == Kind::Func ||
          ikind == Kind::Frame)) {
      throw TypeError() << "Invalid expression of type " << _name_type(ikind)
          << " at index " << i << " in the i-selector list";
    }
    RowIndex ri = inputs[i]->evaluate_i(ctx);
    if (!ri) ri = RowIndex(0, ctx.nrows(), 1);
    rowindices.push_back(std::move(ri));
  }
  return RowIndex::concat(rowindices);
}


static RowIndex _evaluate_i_bools(const vecExpr& inputs, EvalContext& ctx) {
  size_t nrows = ctx.nrows();
  if (inputs.size() != nrows) {
    throw ValueError()
        << "The length of boolean list in i selector does not match the "
           "number of rows in the Frame: " << inputs.size() << " vs " << nrows;
  }
  Buffer databuf = Buffer::mem(nrows * sizeof(int32_t));
  auto data = static_cast<int32_t*>(databuf.xptr());
  size_t data_index = 0;
  for (size_t i = 0; i < nrows; ++i) {
    if (inputs[i]->get_expr_kind() != Kind::Bool) {
      throw TypeError() << "Element " << i << " in the i-selector list is "
          << _name_type(inputs[i]->get_expr_kind()) << ", whereas the previous "
          "elements were boolean";
    }
    bool x = inputs[i]->evaluate_bool();
    if (x) {
      data[data_index++] = static_cast<int32_t>(i);
    }
  }
  databuf.resize(data_index * sizeof(int32_t));
  return RowIndex(std::move(databuf), RowIndex::ARR32|RowIndex::SORTED);
}


static RowIndex _evaluate_i_ints(const vecExpr& inputs, EvalContext& ctx) {
  auto inrows = static_cast<int64_t>(ctx.nrows());
  Buffer databuf = Buffer::mem(inputs.size() * sizeof(int32_t));
  int32_t* data = static_cast<int32_t*>(databuf.xptr());
  size_t data_index = 0;
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto ikind = inputs[i]->get_expr_kind();
    if (ikind == Kind::Int) {
      int64_t x = inputs[i]->evaluate_int();
      if (x < -inrows || x >= inrows) {
        throw ValueError() << "Index " << x << " is invalid for a Frame with "
            << inrows << " rows";
      }
      data[data_index++] = static_cast<int32_t>((x >= 0)? x : x + inrows);
    }
    else if (ikind == Kind::None) {}  // skip
    else if (ikind == Kind::SliceAll || ikind == Kind::SliceInt) {
      return _evaluate_i_other(inputs, ctx);
    }
    else {
      throw TypeError() << "Invalid item of type " << _name_type(ikind)
          << " at index " << i << " in the i-selector list";
    }
  }
  databuf.resize(data_index * sizeof(int32_t));
  return RowIndex(std::move(databuf), RowIndex::ARR32);
}




RowIndex Head_List::evaluate_i(const vecExpr& inputs, EvalContext& ctx) const
{
  if (inputs.empty()) {
    return RowIndex(0, 0, 1);  // Select-nothing rowindex
  }
  auto kind0 = inputs[0]->get_expr_kind();
  if (kind0 == Kind::Bool) return _evaluate_i_bools(inputs, ctx);
  if (kind0 == Kind::Int)  return _evaluate_i_ints(inputs, ctx);
  return _evaluate_i_other(inputs, ctx);
}


RiGb Head_List::evaluate_iby(const vecExpr&, EvalContext&) const {
  throw NotImplError() << "Head_List::evaluate_iby() not implemented yet";
}




//------------------------------------------------------------------------------
// prepare_by
//------------------------------------------------------------------------------

void Head_List::prepare_by(const vecExpr& inputs, EvalContext& ctx,
                           Workframe& outwf, std::vector<SortFlag>& outflags)
                          const
{
  if (inputs.empty()) return;

  auto kind = _resolve_list_kind(inputs);
  if (kind == Kind::Str || kind == Kind::Int) {
    for (const auto& arg : inputs) {
      outwf.cbind( arg->evaluate_f(ctx, 0) );
      outflags.push_back(SortFlag::NONE);
    }
  }
  else if (kind == Kind::Func) {
    for (const auto& arg : inputs) {
      auto negcol = arg->unnegate_column();
      if (negcol) {
        outwf.cbind( negcol->evaluate_n(ctx) );
        outflags.push_back(SortFlag::DESCENDING);
      } else {
        outwf.cbind( arg->evaluate_n(ctx) );
        outflags.push_back(SortFlag::NONE);
      }
    }
  }
  else {
    throw TypeError() << "Sequence of " << _name_type(kind) << " expressions "
        "cannot be used in a by() clause";
  }
}





}}  // namespace dt::expr
