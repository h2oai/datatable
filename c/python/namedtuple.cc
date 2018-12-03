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
#include "python/namedtuple.h"
#include "python/list.h"
#include "python/tuple.h"
#include "python/string.h"
#include "python/int.h"

namespace py {

/**
*  Create a namedtuple type based on the collections.namedtuple datatype.
*  An alternative is to use `Struct Sequence Objects` as outlined here
*  https://docs.python.org/3/c-api/tuple.html
*  However, objects created with `PyStructSequence_New` are not standard
*  Python namedtuples, but rather a simpler version of the latter.
*/
onamedtupletype::onamedtupletype(strpair tuple_info,
                                 std::vector<strpair> fields_info) {
  auto itemgetter = py::oobj::import("operator", "itemgetter");
  auto namedtuple = py::oobj::import("collections", "namedtuple");
  auto property   = py::oobj::import("builtins", "property");

  // Create a namedtuple type from the supplied fields
  nfields = fields_info.size();
  py::olist argnames(nfields);
  for (size_t i = 0; i < nfields; ++i) {
    argnames.set(i, py::ostring(fields_info[i].first));
  }

  py::otuple args(2);
  args.set(0, py::ostring(tuple_info.first));
  args.set(1, argnames);

  auto type = namedtuple.call(args);
  auto res = std::move(type).release();

  // Set namedtuple doc
  PyObject_SetAttrString(res, "__doc__",
                         py::ostring(tuple_info.second).to_borrowed_ref());

  // Set field docs
  py::otuple args_prop(4);
  py::otuple args_itemgetter(1);
  args_prop.set(1, py::None());
  args_prop.set(2, py::None());
  for (size_t i = 0; i < nfields; ++i) {
    args_itemgetter.set(0, py::oint(i));
    args_prop.set(0, itemgetter.call(args_itemgetter));
    args_prop.set(3, py::ostring(fields_info[i].second));
    auto prop = property.call(args_prop);
    PyObject_SetAttrString(res, fields_info[i].first,
                           prop.to_borrowed_ref());
  }

  // Convert `PyObject*` to `PyTypeObject*`, so that it can be passed to
  // `py::onamedtuple` constructor
  v = reinterpret_cast<PyTypeObject*>(res);
}


onamedtupletype::~onamedtupletype() {
  Py_XDECREF(v);
}


onamedtuple::onamedtuple(onamedtupletype* type) {
  v = PyTuple_New(static_cast<int64_t>(type->nfields));
  if (!v) throw PyError();
  Py_TYPE(v) = type->v;
}


}  // namespace py
