//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "expr/by_node.h"
#include "expr/collist.h"
#include "expr/expr_column.h"
#include "expr/eval_context.h"
#include "python/arg.h"
#include "python/tuple.h"
#include "utils/exceptions.h"
#include "datatablemodule.h"
#include "sort.h"
namespace dt {


//------------------------------------------------------------------------------
// dt::by_node
//------------------------------------------------------------------------------

by_node::column_descriptor::column_descriptor(
    size_t i, std::string&& name_, bool desc, bool sort)
  : index(i), name(std::move(name_)), descending(desc), sort_only(sort) {}

by_node::column_descriptor::column_descriptor(
    exprptr&& e, std::string&& name_, bool desc, bool sort)
  : index(size_t(-1)), expr(std::move(e)), name(std::move(name_)),
    descending(desc), sort_only(sort) {}


by_node::by_node() {
  n_group_columns = 0;
  TRACK(this, sizeof(*this), "by_node");
}

by_node::~by_node() {
  UNTRACK(this);
}


void by_node::add_groupby_columns(EvalContext& ctx, collist_ptr&& cl) {
  _add_columns(ctx, std::move(cl), true);
}

void by_node::add_sortby_columns(EvalContext& ctx, collist_ptr&& cl) {
  _add_columns(ctx, std::move(cl), false);
}


void by_node::_add_columns(EvalContext& ctx, collist_ptr&& cl, bool isgrp) {
  strvec names = cl->release_names();
  bool has_names = !names.empty();
  if (cl->is_simple_list()) {
    intvec indices = cl->release_indices();
    size_t n = indices.size();
    for (size_t i = 0; i < n; ++i) {
      cols.emplace_back(
          indices[i],
          has_names? std::move(names[i]) : std::string(),
          false,          // descending
          !isgrp          // sort_only
      );
    }
    n_group_columns += isgrp * n;
  }
  else {
    using pexpr = std::unique_ptr<dt::expr::base_expr>;
    exprvec exprs = cl->release_exprs();
    size_t n = exprs.size();
    size_t n_computed = 0;
    for (size_t i = 0; i < n; ++i) {
      bool descending = false;
      pexpr cexpr = std::move(exprs[i]);
      pexpr neg = cexpr->get_negated_expr();
      if (neg) {
        auto colexpr = dynamic_cast<dt::expr::expr_column*>(neg.get());
        size_t j = colexpr->get_col_index(ctx);
        if (j != size_t(-1)) {
          cols.emplace_back(
              j,
              has_names? std::move(names[i]) : std::string(),
              true,   // descending
              !isgrp  // sort_only
          );
          continue;
        }
        cexpr = std::move(neg);
        descending = true;
      }
      cols.emplace_back(
          std::move(cexpr),
          has_names? std::move(names[i]) : std::string(),
          descending,
          !isgrp  // sort_only
      );
      n_computed++;
    }
    n_group_columns += isgrp * n;
    if (n_computed) {
      throw NotImplError() << "Computed columns cannot be used in groupby/sort";
    }
  }
}


by_node::operator bool() const {
  return (n_group_columns > 0);
}


bool by_node::has_group_column(size_t i) const {
  for (auto& col : cols) {
    if (col.index == i && !col.sort_only) return true;
  }
  return false;
}


void by_node::create_columns(EvalContext& ctx) {
  DataTable* dt0 = ctx.get_datatable(0);
  RowIndex ri0 = ctx.get_rowindex(0);
  if (ctx.get_groupby_mode() == GroupbyMode::GtoONE) {
    ri0 = RowIndex(arr32_t(ctx.gb.ngroups(), ctx.gb.offsets_r()), true) * ri0;
  }

  auto dt0_names = dt0->get_names();
  for (auto& col : cols) {
    if (col.sort_only) continue;
    size_t j = col.index;
    xassert(j != size_t(-1));
    Column newcol = dt0->get_column(j);  // copy
    ctx.add_column(std::move(newcol), ri0,
                  col.name.empty()? dt0_names[j] : std::move(col.name));
  }
}


void by_node::execute(EvalContext& ctx) const {
  if (cols.empty()) {
    ctx.gb = Groupby::single_group(ctx.nrows());
    return;
  }
  const DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  if (ri0) {
    throw NotImplError() << "Groupby/sort cannot be combined with i expression";
  }
  if (dt0->nrows() == 0) {
    // When grouping a Frame with 0 rows, produce a no-groups Groupby
    return;
  }
  std::vector<Column> sort_cols;
  std::vector<SortFlag> sort_flags;
  sort_cols.reserve(cols.size());
  sort_flags.reserve(cols.size());
  if (n_group_columns > 0) {
    for (const auto& col : cols) {
      if (col.sort_only) continue;
      sort_cols.push_back(dt0->get_column(col.index));
      sort_flags.push_back(col.descending? SortFlag::DESCENDING : SortFlag::NONE);
    }
  }
  if (n_group_columns < cols.size()) {
    for (auto& col : cols) {
      if (!col.sort_only) continue;
      sort_cols.push_back(dt0->get_column(col.index));
      sort_flags.push_back((col.descending? SortFlag::DESCENDING : SortFlag::NONE)
                           | SortFlag::SORT_ONLY);
    }
  }
  // if (n_group_columns) {
    auto res = group(sort_cols, sort_flags);
    ctx.gb = std::move(res.second);
    ctx.apply_rowindex(res.first);
  // } else {
  //   auto res = dt0->sort(spec);
  //   ctx.apply_rowindex(res);
  // }
}




}  // namespace dt
//------------------------------------------------------------------------------
// py::oby
//------------------------------------------------------------------------------
namespace py {


oby::oby(const robj& src) : oobj(src) {}
oby::oby(const oobj& src) : oobj(src) {}


oby oby::make(const robj& r) {
  return oby(oby::oby_pyobject::make(r));
}


bool oby::check(PyObject* v) {
  return oby::oby_pyobject::check(v);
}


void oby::init(PyObject* m) {
  oby::oby_pyobject::init_type(m);
}


dt::collist_ptr oby::cols(dt::EvalContext& ctx) const {
  // robj cols = reinterpret_cast<const pyobj*>(v)->cols;
  robj cols = reinterpret_cast<const oby::oby_pyobject*>(v)->get_cols();
  return dt::collist_ptr(new dt::collist(ctx, cols, dt::collist::BY_NODE));
}




static PKArgs args___init__(0, 0, 0, true, false, {}, "__init__", nullptr);


void oby::oby_pyobject::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.by");
  xt.set_class_doc("by() clause for use in DT[i, j, ...]");
  xt.set_subclassable(true);

  xt.add(CONSTRUCTOR(&oby::oby_pyobject::m__init__, args___init__));
  xt.add(DESTRUCTOR(&oby::oby_pyobject::m__dealloc__));

  static GSArgs args__cols("_cols");
  xt.add(GETTER(&oby::oby_pyobject::get_cols, args__cols));
}


void oby::oby_pyobject::m__init__(const PKArgs& args) {
  olist colslist(args.num_vararg_args());
  size_t i = 0;
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  cols = std::move(colslist);
}


void oby::oby_pyobject::m__dealloc__() {
  cols = nullptr;  // Releases the stored oobj
}


oobj oby::oby_pyobject::get_cols() const {
  return cols;
}



}  // namespace py
