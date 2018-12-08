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


onamedtupletype::onamedtupletype(
  const std::string& cls_name, const strvec& field_names
) : onamedtupletype(cls_name, "", field_names, {}) {}


/**
*  Create a namedtuple type based on the collections.namedtuple datatype.
*  An alternative is to use `Struct Sequence Objects` as outlined here
*  https://docs.python.org/3/c-api/tuple.html
*  However, objects created with `PyStructSequence_New` are not standard
*  Python namedtuples, but rather a simpler version of the latter.
*/
onamedtupletype::onamedtupletype(const std::string& cls_name,
                                 const std::string& cls_doc,
                                 const strvec& field_names,
                                 const strvec& field_docs)
{
  auto itemgetter = py::oobj::import("operator", "itemgetter");
  auto namedtuple = py::oobj::import("collections", "namedtuple");
  auto property   = py::oobj::import("builtins", "property");

  // Create a namedtuple type from the supplied fields
  nfields = field_names.size();
  py::olist argnames(nfields);
  for (size_t i = 0; i < nfields; ++i) {
    argnames.set(i, py::ostring(field_names[i]));
  }

  py::otuple args(2);
  args.set(0, py::ostring(cls_name));
  args.set(1, argnames);

  auto type = namedtuple.call(args);
  auto res = std::move(type).release();

  // Set namedtuple doc
  py::ostring docstr = py::ostring(cls_doc);
  PyObject_SetAttrString(res, "__doc__",
                         docstr.to_borrowed_ref());

  // Set field docs
  py::otuple args_prop(4);
  py::otuple args_itemgetter(1);
  args_prop.set(1, py::None());
  args_prop.set(2, py::None());
  for (size_t i = 0; i < field_docs.size(); ++i) {
    args_itemgetter.set(0, py::oint(i));
    args_prop.set(0, itemgetter.call(args_itemgetter));
    args_prop.set(3, py::ostring(field_docs[i]));
    auto prop = property.call(args_prop);
    PyObject_SetAttrString(res, field_names[i].c_str(),
                           prop.to_borrowed_ref());
  }

  // Convert `PyObject*` to `PyTypeObject*`, so that it can be passed to
  // `py::onamedtuple` constructor
  v = reinterpret_cast<PyTypeObject*>(res);
}


onamedtupletype::~onamedtupletype() {
  Py_XDECREF(v);
}


onamedtuple::onamedtuple(const onamedtupletype& type) {
  v = PyTuple_New(static_cast<Py_ssize_t>(type.nfields));
  if (!v) throw PyError();

  // Set the new type and adjust number of references as needed
  Py_DECREF(Py_TYPE(v));
  Py_TYPE(v) = type.v;
  Py_INCREF(type.v);
}


}  // namespace py
