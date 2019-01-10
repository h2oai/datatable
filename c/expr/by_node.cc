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
#include "python/arg.h"
#include "python/tuple.h"
namespace py {



//------------------------------------------------------------------------------
// oby::pyobj::Type
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
// oby::pyobj
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
// oby
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





}
