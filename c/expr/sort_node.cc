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
#include "expr/collist.h"
#include "expr/expr.h"
#include "expr/sort_node.h"
#include "python/_all.h"
#include "utils/exceptions.h"

namespace py {



//------------------------------------------------------------------------------
// py::_sort
//------------------------------------------------------------------------------

static PKArgs args___init__(
    0, 0, 0, true, false, {}, "__init__", nullptr);

void _sort::m__init__(const PKArgs& args) {
  olist colslist(args.num_vararg_args());
  size_t i = 0;
  for (robj arg : args.varargs()) {
    colslist.set(i++, arg);
  }
  cols = std::move(colslist);
}


void _sort::m__dealloc__() {
  cols = nullptr;  // Releases the stored oobj
}


oobj _sort::get_cols() const {
  return cols;
}


void _sort::impl_init_type(XTypeMaker& xt) {
  xt.set_class_name("datatable.sort");
  xt.set_class_doc("sort() clause for use in DT[i, j, ...]");

  xt.add(CONSTRUCTOR(&_sort::m__init__, args___init__));
  xt.add(DESTRUCTOR(&_sort::m__dealloc__));
  static GSArgs args__cols("_cols");
  xt.add(GETTER(&_sort::get_cols, args__cols));
}




//------------------------------------------------------------------------------
// py::osort
//------------------------------------------------------------------------------

osort::osort(const robj& src) : oobj(src) {}
osort::osort(const oobj& src) : oobj(src) {}

osort::osort(const otuple& cols) {
  PyObject* sort_class = reinterpret_cast<PyObject*>(&py::_sort::type);
  v = PyObject_CallObject(sort_class, cols.to_borrowed_ref());
  if (!v) throw PyError();
}


bool osort::check(PyObject* v) {
  return _sort::check(v);
}


void osort::init(PyObject* m) {
  _sort::init_type(m);
}


dt::collist_ptr osort::cols(dt::workframe& wf) const {
  robj cols = reinterpret_cast<const _sort*>(v)->cols;
  return dt::collist_ptr(
            new dt::collist(wf, cols, dt::collist::SORT_NODE));
}



}  // namespace py
