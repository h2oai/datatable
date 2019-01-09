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
#include "python/arg.h"
#include "python/tuple.h"
#include "utils/exceptions.h"


//------------------------------------------------------------------------------
// dt::by_node
//------------------------------------------------------------------------------
namespace dt {

class collist_bn : public by_node {
  intvec indices;
  strvec names;

  public:
    collist_bn(cols_intlist*);
    void execute(workframe& wf) override;
};

class exprlist_bn : public by_node {
  exprvec exprs;
  strvec names;

  public:
    exprlist_bn(cols_exprlist*);
    void execute(workframe& wf) override;
};


by_node::~by_node() {}

collist_bn::collist_bn(cols_intlist* cl)
  : indices(std::move(cl->indices)), names(std::move(cl->names)) {}

exprlist_bn::exprlist_bn(cols_exprlist* cl)
  : exprs(std::move(cl->exprs)), names(std::move(cl->names)) {}


void collist_bn::execute(workframe& wf) {
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    // TODO
  }
  RowIndex ri = dt0->sortby(indices, &wf.get_groupby_ref());
  wf.apply_rowindex(ri);
}


void exprlist_bn::execute(workframe&) {
  throw NotImplError();
}


}  // namespace dt


//------------------------------------------------------------------------------
// py::oby::pyobj::Type
//------------------------------------------------------------------------------

py::PKArgs py::oby::pyobj::Type::args___init__(
    0, 0, 0, true, false, {}, "__init__", nullptr);

const char* py::oby::pyobj::Type::classname() {
  return "datatable.by";
}

const char* py::oby::pyobj::Type::classdoc() {
  return "by() clause for use in DT[i, j, ...]";
}

bool py::oby::pyobj::Type::is_subclassable() {
  return true;  // TODO: make false
}

void py::oby::pyobj::Type::init_methods_and_getsets(Methods&, GetSetters& gs) {
  gs.add<&pyobj::get_cols>("_cols");
}




//------------------------------------------------------------------------------
// py::oby::pyobj
//------------------------------------------------------------------------------

void py::oby::pyobj::m__init__(PKArgs& args) {
  olist colslist(args.num_vararg_args());
  size_t i = 0;
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  cols = std::move(colslist);
}


void py::oby::pyobj::m__dealloc__() {
  cols = nullptr;  // Releases the stored oobj
}


py::oobj py::oby::pyobj::get_cols() const {
  return cols;
}




//------------------------------------------------------------------------------
// py::oby
//------------------------------------------------------------------------------

py::oby::oby(const robj& src) : oobj(src) {}
py::oby::oby(const oobj& src) : oobj(src) {}


py::oby py::oby::make(const robj& r) {
  robj oby_type(reinterpret_cast<PyObject*>(&pyobj::Type::type));
  otuple args(1);
  args.set(0, r);
  return oby(oby_type.call(args));
}


bool py::oby::check(PyObject* v) {
  if (!v) return false;
  auto typeptr = reinterpret_cast<PyObject*>(&pyobj::Type::type);
  int ret = PyObject_IsInstance(v, typeptr);
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}


void py::oby::init(PyObject* m) {
  pyobj::Type::init(m);
}


by_node_ptr py::oby::to_by_node(dt::workframe& wf) const {
  robj cols = reinterpret_cast<const pyobj*>(v)->cols;
  auto cl = dt::collist::make(wf, cols, "`by`");
  auto cl_int = dynamic_cast<dt::cols_intlist*>(cl.get());
  auto cl_expr = dynamic_cast<dt::cols_exprlist*>(cl.get());
  xassert(cl_int || cl_expr);
  return cl_int? by_node_ptr(new dt::collist_bn(cl_int))
               : by_node_ptr(new dt::exprlist_bn(cl_expr));
}
