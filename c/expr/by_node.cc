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
#include "expr/by_node.h"
#include "expr/collist.h"
#include "expr/py_expr.h"
#include "python/arg.h"
#include "python/tuple.h"
#include "utils/exceptions.h"

namespace dt {


//------------------------------------------------------------------------------
// dt::by_node
//------------------------------------------------------------------------------

by_node::by_node() {
  n_group_columns = 0;
}


void by_node::add_groupby_columns(collist_ptr&& cl) {
  _add_columns(std::move(cl), true);
}


void by_node::add_sortby_columns(collist_ptr&& cl) {
  _add_columns(std::move(cl), false);
}


void by_node::_add_columns(collist_ptr&& cl, bool group_columns) {
  auto cl_int  = dynamic_cast<cols_intlist*>(cl.get());
  auto cl_expr = dynamic_cast<cols_exprlist*>(cl.get());
  xassert(cl_int || cl_expr);
  if (cl_int) {
    bool has_names = !cl_int->names.empty();
    size_t n = cl_int->indices.size();
    for (size_t i = 0; i < n; ++i) {
      _add_column(cl_int->indices[i],
                  has_names? std::move(cl_int->names[i]) : std::string(),
                  group_columns);
    }
  }
  if (cl_expr) {
    bool has_names = !cl_expr->names.empty();
    size_t n = cl_expr->exprs.size();
    for (size_t i = 0; i < n; ++i) {
      _add_column(std::move(cl_expr->exprs[i]),
                  has_names? std::move(cl_expr->names[i]) : std::string(),
                  group_columns);
    }
    throw NotImplError() << "Computed columns cannot be used in groupby/sort";
  }
}


void by_node::_add_column(size_t index, std::string&& name, bool grp) {
  cols.push_back({index, nullptr, std::move(name), grp, false});
  n_group_columns += grp;
}


void by_node::_add_column(exprptr&& expr, std::string&& name, bool grp) {
  cols.push_back({size_t(-1), std::move(expr), std::move(name), grp, false});
  n_group_columns += grp;
}


by_node::operator bool() const {
  return (n_group_columns > 0);
}


bool by_node::has_column(size_t i) const {
  for (auto& col : cols) {
    if (col.index == i) return true;
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
    size_t j = col.index;
    xassert(j != size_t(-1));
    Column* colj = dt0->columns[j]->shallowcopy();
    wf.add_column(colj, ri0, col.name.empty()? dt0_names[j]
                                             : std::move(col.name));
  }
}


void by_node::execute(workframe& wf) const {
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    throw NotImplError() << "Groupby/sort cannot be combined with i expression";
  }
  std::vector<sort_spec> spec;
  for (auto& col : cols) {
    spec.push_back(sort_spec(col.index));
  }
  if (!spec.empty()) {
    auto res = dt0->group(spec);
    wf.gb = std::move(res.second);
    wf.apply_rowindex(res.first);
  }
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
  gs.add<&pyobj::get_cols>("_cols");
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
  otuple args(1);
  args.set(0, r);
  return oby(oby_type.call(args));
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
