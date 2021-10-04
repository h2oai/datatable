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
#include "expr/fexpr_list.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include <algorithm>             // std::sort
#include <iostream>
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

FExpr_List::FExpr_List(vecExpr&& args)
  : args_(std::move(args))
{}

ptrExpr FExpr_List::empty() {
  return ptrExpr(new FExpr_List());
}

ptrExpr FExpr_List::make(py::robj src) {
  vecExpr args;
  if (src.is_list_or_tuple()) {
    auto srclist = src.to_pylist();
    args.reserve(srclist.size());
    for (size_t i = 0; i < srclist.size(); ++i) {
      args.emplace_back(as_fexpr(srclist[i]));
    }
  }
  else {
    for (auto elem : src.to_oiter()) {
      args.emplace_back(as_fexpr(elem));
    }
  }
  return ptrExpr(new FExpr_List(std::move(args)));
}

void FExpr_List::add_expr(ptrExpr&& expr) {
  args_.push_back(std::move(expr));
}



//------------------------------------------------------------------------------
// Evaluation
//------------------------------------------------------------------------------

Workframe FExpr_List::evaluate_n(EvalContext& ctx) const {
  Workframe outputs(ctx);
  for (const auto& arg : args_) {
    outputs.cbind( arg->evaluate_n(ctx) );
  }
  return outputs;
}


// Evaluate list as a replacement target when replacing columns at
// `indices` within the "root" Frame.
//
Workframe FExpr_List::evaluate_r(
    EvalContext& ctx, const sztvec& indices) const
{
  Workframe outputs(ctx);
  if (args_.size() == indices.size()) {
    for (size_t i = 0; i < args_.size(); ++i) {
      outputs.cbind( args_[i]->evaluate_r(ctx, {indices[i]}) );
    }
  }
  else if (args_.size() == 1) {
    for (size_t i = 0; i < indices.size(); ++i) {
      outputs.cbind( args_[0]->evaluate_r(ctx, {indices[i]}) );
    }
  }
  else {
    throw ValueError() << "The LHS of the replacement has " << indices.size()
        << " columns, while the RHS has " << args_.size()
        << " replacement expressions";
  }
  return outputs;
}



Workframe FExpr_List::evaluate_f(EvalContext& ctx, size_t i) const {
  Workframe outputs(ctx);
  for (const auto& arg : args_) {
    outputs.cbind( arg->evaluate_f(ctx, i) );
  }
  return outputs;
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
static Kind _resolve_list_kind(const vecExpr& args) {
  auto listkind = Kind::Unknown;
  for (size_t i = 0; i < args.size(); ++i) {
    auto kind = args[i]->get_expr_kind();
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


static Workframe _evaluate_bool_list(const vecExpr& args, EvalContext& ctx) {
  DataTable* df = ctx.get_datatable(0);
  if (args.size() != df->ncols()) {
    throw ValueError()
        << "The length of boolean list in `j` selector does not match the "
           "number of columns in the Frame: "
        << args.size() << " vs " << df->ncols();
  }
  Workframe outputs(ctx);
  for (size_t i = 0; i < args.size(); ++i) {
    bool x = args[i]->evaluate_bool();
    if (x) outputs.add_ref_column(0, i);
  }
  return outputs;
}


Workframe FExpr_List::evaluate_j(EvalContext& ctx) const {
  auto kind = _resolve_list_kind(args_);
  if (kind == Kind::Bool) {
    return _evaluate_bool_list(args_, ctx);
  }

  Workframe outputs(ctx);
  for (const auto& arg : args_) {
    outputs.cbind( arg->evaluate_j(ctx) );
  }
  return outputs;
}



//------------------------------------------------------------------------------
// i-evaluation
//------------------------------------------------------------------------------

static RowIndex _evaluate_i_other(const vecExpr& args, EvalContext& ctx) {
  std::vector<RowIndex> rowindices;
  for (size_t i = 0; i < args.size(); ++i) {
    auto ikind = args[i]->get_expr_kind();
    if (ikind == Kind::None) continue;
    if (!(ikind == Kind::Int || ikind == Kind::SliceInt ||
          ikind == Kind::SliceAll || ikind == Kind::Func ||
          ikind == Kind::Frame)) {
      throw TypeError() << "Invalid expression of type " << _name_type(ikind)
          << " at index " << i << " in the i-selector list";
    }
    RowIndex ri = args[i]->evaluate_i(ctx);
    if (!ri) ri = RowIndex(0, ctx.nrows(), 1);
    rowindices.push_back(std::move(ri));
  }
  return RowIndex::concat(rowindices);
}


static RowIndex _evaluate_i_bools(const vecExpr& args, EvalContext& ctx) {
  size_t nrows = ctx.nrows();
  if (args.size() != nrows) {
    throw ValueError()
        << "The length of boolean list in i selector does not match the "
           "number of rows in the Frame: " << args.size() << " vs " << nrows;
  }
  Buffer databuf = Buffer::mem(nrows * sizeof(int32_t));
  auto data = static_cast<int32_t*>(databuf.xptr());
  size_t data_index = 0;
  for (size_t i = 0; i < nrows; ++i) {
    if (args[i]->get_expr_kind() != Kind::Bool) {
      throw TypeError() << "Element " << i << " in the i-selector list is "
          << _name_type(args[i]->get_expr_kind()) << ", whereas the previous "
          "elements were boolean";
    }
    bool x = args[i]->evaluate_bool();
    if (x) {
      data[data_index++] = static_cast<int32_t>(i);
    }
  }
  databuf.resize(data_index * sizeof(int32_t));
  return RowIndex(std::move(databuf), RowIndex::ARR32|RowIndex::SORTED);
}


static RowIndex _evaluate_i_ints(const vecExpr& args, EvalContext& ctx) {
  auto inrows = static_cast<int64_t>(ctx.nrows());
  Buffer databuf = Buffer::mem(args.size() * sizeof(int32_t));
  int32_t* data = static_cast<int32_t*>(databuf.xptr());
  bool is_list_to_negate_sorted = true;
  EvalMode eval_mode = ctx.get_mode();
  int64_t max_value = args[0]->evaluate_int();
  size_t data_index = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    auto ikind = args[i]->get_expr_kind();
    if (ikind == Kind::Int) {
      int64_t x = args[i]->evaluate_int();
      if (x < -inrows || x >= inrows) {
        throw ValueError() << "Index " << x << " is invalid for a Frame with "
            << inrows << " rows";
      }
      if (eval_mode == EvalMode::DELETE && is_list_to_negate_sorted) {
        if (x >= max_value) max_value = x; else is_list_to_negate_sorted = false;
      }
      data[data_index++] = static_cast<int32_t>((x >= 0)? x : x + inrows);
    }
    else if (ikind == Kind::None) {}  // skip
    else if (ikind == Kind::SliceAll || ikind == Kind::SliceInt) {
      return _evaluate_i_other(args, ctx);
    }
    else {
      throw TypeError() << "Invalid item of type " << _name_type(ikind)
          << " at index " << i << " in the i-selector list";
    }
  }
  databuf.resize(data_index * sizeof(int32_t));
  if (!is_list_to_negate_sorted) std::sort(data, data + data_index);
  return RowIndex(std::move(databuf), RowIndex::ARR32);
}




RowIndex FExpr_List::evaluate_i(EvalContext& ctx) const {
  if (args_.empty()) {
    return RowIndex(0, 0, 1);  // Select-nothing rowindex
  }
  auto kind0 = args_[0]->get_expr_kind();
  if (kind0 == Kind::Bool) return _evaluate_i_bools(args_, ctx);
  if (kind0 == Kind::Int)  return _evaluate_i_ints(args_, ctx);
  return _evaluate_i_other(args_, ctx);
}


RiGb FExpr_List::evaluate_iby(EvalContext&) const {
  throw NotImplError() << "FExpr_List::evaluate_iby() not implemented yet";
}

void FExpr_List::set_outflags(
    EvalContext& ctx, std::vector<SortFlag>& outflags, bool reverse) const
{
  if (ctx.get_mod_type() == ModType::BY) {
    outflags.push_back(SortFlag::NONE);
  } else {
    outflags.push_back(reverse ? SortFlag::DESCENDING : SortFlag::NONE);
  }
}

void FExpr_List::prepare_by(
    EvalContext& ctx, Workframe& outwf, std::vector<SortFlag>& outflags) const
{
  if (args_.empty()) return;

  auto kind = _resolve_list_kind(args_);
  if (kind == Kind::Str || kind == Kind::Int) {
    size_t id = 0;
    for (const auto& arg : args_) {
      bool reverse = (ctx.get_mod_type() == ModType::BY) ? false : ctx.reverse_sort(id);
//      std::cout << "The reverse flag in top : " << reverse << std::endl;
      outwf.cbind( arg->evaluate_f(ctx, 0) );
      set_outflags(ctx, outflags, reverse);
      ++id;
    }
  }
  else if (kind == Kind::Func) {
    size_t id = 0;
    for (const auto& arg : args_) {
      bool reverse = (ctx.get_mod_type() == ModType::BY) ? false : ctx.reverse_sort(id);
//      std::cout << "The reverse flag in bottom : " << reverse << std::endl;
      auto negcol = arg->unnegate_column();
      if (negcol) {
        outwf.cbind( negcol->evaluate_n(ctx) );
        //if (ctx.get_mod_type() == ModType::BY) {
        //  reverse = false;
        //}
        outflags.push_back(reverse ? SortFlag::NONE : SortFlag::DESCENDING);
      } else {
        outwf.cbind( arg->evaluate_n(ctx) );
        set_outflags(ctx, outflags, reverse);
      }
      ++id;
    }
  }
  else {
    throw TypeError() << "Sequence of " << _name_type(kind) << " expressions "
        "cannot be used in a by() clause";
  }
}




//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

Kind FExpr_List::get_expr_kind() const {
  return Kind::List;
}


int FExpr_List::precedence() const noexcept {
  return 17;
}


std::string FExpr_List::repr() const {
  std::string out = "[";
  for (size_t i = 0; i < args_.size(); ++i) {
    if (i) out += ", ";
    out += args_[i]->repr();
  }
  out += ']';
  return out;
}




}}  // namespace dt::expr
