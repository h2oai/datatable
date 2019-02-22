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
#include "expr/base_expr.h"
#include "expr/by_node.h"
#include "expr/collist.h"
#include "expr/py_expr.h"
#include "expr/workframe.h"
#include "python/arg.h"
#include "python/tuple.h"
#include "utils/exceptions.h"
#include "datatablemodule.h"
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


void by_node::add_groupby_columns(workframe& wf, collist_ptr&& cl) {
  _add_columns(wf, std::move(cl), true);
}

void by_node::add_sortby_columns(workframe& wf, collist_ptr&& cl) {
  _add_columns(wf, std::move(cl), false);
}


void by_node::_add_columns(workframe& wf, collist_ptr&& cl, bool isgrp) {
  auto cl_int  = dynamic_cast<cols_intlist*>(cl.get());
  auto cl_expr = dynamic_cast<cols_exprlist*>(cl.get());
  xassert(cl_int || cl_expr);
  if (cl_int) {
    bool has_names = !cl_int->names.empty();
    size_t n = cl_int->indices.size();
    for (size_t i = 0; i < n; ++i) {
      cols.emplace_back(
          cl_int->indices[i],
          has_names? std::move(cl_int->names[i]) : std::string(),
          false,          // descending
          !isgrp  // sort_only
      );
    }
    n_group_columns += isgrp * n;
  }
  if (cl_expr) {
    bool has_names = !cl_expr->names.empty();
    size_t n = cl_expr->exprs.size();
    size_t n_computed = 0;
    for (size_t i = 0; i < n; ++i) {
      bool descending = false;
      pexpr cexpr = std::move(cl_expr->exprs[i]);
      pexpr neg = cexpr->get_negated_expr();
      if (neg) {
        size_t j = neg->get_col_index(wf);
        if (j != size_t(-1)) {
          cols.emplace_back(
              j,
              has_names? std::move(cl_expr->names[i]) : std::string(),
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
          has_names? std::move(cl_expr->names[i]) : std::string(),
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


void by_node::create_columns(workframe& wf) {
  DataTable* dt0 = wf.get_datatable(0);
  RowIndex ri0 = wf.get_rowindex(0);
  if (wf.get_groupby_mode() == GroupbyMode::GtoONE) {
    ri0 = RowIndex(arr32_t(wf.gb.ngroups(), wf.gb.offsets_r()), true) * ri0;
  }

  auto dt0_names = dt0->get_names();
  for (auto& col : cols) {
    if (col.sort_only) continue;
    size_t j = col.index;
    xassert(j != size_t(-1));
    Column* colj = dt0->columns[j]->shallowcopy();
    wf.add_column(colj, ri0, col.name.empty()? dt0_names[j]
                                             : std::move(col.name));
  }
}


void by_node::execute(workframe& wf) const {
  if (cols.empty()) return;
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    throw NotImplError() << "Groupby/sort cannot be combined with i expression";
  }
  std::vector<sort_spec> spec;
  spec.reserve(cols.size());
  if (n_group_columns > 0) {
    for (auto& col : cols) {
      if (col.sort_only) continue;
      spec.emplace_back(col.index, col.descending, false, false);
    }
  }
  if (n_group_columns < cols.size()) {
    for (auto& col : cols) {
      if (!col.sort_only) continue;
      spec.emplace_back(col.index, col.descending, false, true);
    }
  }
  // if (n_group_columns) {
    auto res = dt0->group(spec);
    wf.gb = std::move(res.second);
    wf.apply_rowindex(res.first);
  // } else {
  //   auto res = dt0->sort(spec);
  //   wf.apply_rowindex(res);
  // }
}




}  // namespace dt
namespace py {


//------------------------------------------------------------------------------
// py::oby::pyobj::Type
//------------------------------------------------------------------------------

PKArgs oby::pyobj::Type::args___init__(
    0, 0, 0, true, false, {}, "__init__", nullptr);

const char* oby::pyobj::Type::classname() {
  return "datatable.by";
}

const char* oby::pyobj::Type::classdoc() {
  return "by() clause for use in DT[i, j, ...]";
}

bool oby::pyobj::Type::is_subclassable() {
  return true;  // TODO: make false
}

void oby::pyobj::Type::init_methods_and_getsets(Methods&, GetSetters& gs) {
  static GSArgs args__cols("_cols");
  ADD_GETTER(gs, &pyobj::get_cols, args__cols);
}




//------------------------------------------------------------------------------
// py::oby::pyobj
//------------------------------------------------------------------------------

void oby::pyobj::m__init__(PKArgs& args) {
  olist colslist(args.num_vararg_args());
  size_t i = 0;
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  cols = std::move(colslist);
}


void oby::pyobj::m__dealloc__() {
  cols = nullptr;  // Releases the stored oobj
}


oobj oby::pyobj::get_cols() const {
  return cols;
}




//------------------------------------------------------------------------------
// py::oby
//------------------------------------------------------------------------------

oby::oby(const robj& src) : oobj(src) {}
oby::oby(const oobj& src) : oobj(src) {}


oby oby::make(const robj& r) {
  robj oby_type(reinterpret_cast<PyObject*>(&pyobj::Type::type));
  return oby(oby_type.call({r}));
}


bool oby::check(PyObject* v) {
  if (!v) return false;
  auto typeptr = reinterpret_cast<PyObject*>(&pyobj::Type::type);
  int ret = PyObject_IsInstance(v, typeptr);
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}


void oby::init(PyObject* m) {
  pyobj::Type::init(m);
}


dt::collist_ptr oby::cols(dt::workframe& wf) const {
  robj cols = reinterpret_cast<const pyobj*>(v)->cols;
  return dt::collist::make(wf, cols, "`by`");
}



}  // namespace py
