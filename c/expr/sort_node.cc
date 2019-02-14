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
#include "expr/sort_node.h"
#include "expr/collist.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// py::_sort::Type
//------------------------------------------------------------------------------

PKArgs _sort::Type::args___init__(
    0, 0, 0, true, false, {}, "__init__", nullptr);

const char* _sort::Type::classname() {
  return "datatable.sort";
}

const char* _sort::Type::classdoc() {
  return "sort() clause for use in DT[i, j, ...]";
}

bool _sort::Type::is_subclassable() {
  return false;
}

void _sort::Type::init_methods_and_getsets(Methods&, GetSetters& gs) {
  static GSArgs args__cols("_cols");
  ADD_GETTER(gs, &_sort::get_cols, args__cols);
}




//------------------------------------------------------------------------------
// py::_sort
//------------------------------------------------------------------------------

void _sort::m__init__(PKArgs& args) {
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




//------------------------------------------------------------------------------
// py::osort
//------------------------------------------------------------------------------

osort::osort(const robj& src) : oobj(src) {}
osort::osort(const oobj& src) : oobj(src) {}


bool osort::check(PyObject* v) {
  if (!v) return false;
  int ret = PyObject_IsInstance(v,
              reinterpret_cast<PyObject*>(&py::_sort::Type::type));
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}


void osort::init(PyObject* m) {
  _sort::Type::init(m);
}


dt::collist_ptr osort::cols(dt::workframe& wf) const {
  robj cols = reinterpret_cast<const _sort*>(v)->cols;
  return dt::collist::make(wf, cols, "`sort`");
}



}  // namespace py
