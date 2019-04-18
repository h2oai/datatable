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
#include "utils/assert.h"

namespace py {


//------------------------------------------------------------------------------
// onamedtupletype
//------------------------------------------------------------------------------

onamedtupletype::onamedtupletype(const std::string& cls_name,
                                 const strvec& field_names)
  : onamedtupletype(cls_name, "",
                    std::vector<field>(field_names.begin(), field_names.end()))
{}


/**
 * Create a namedtuple using python function `collections.namedtuple()`.
 *
 * Note: we do not use PyStructSequence objects from Python C API, because
 * they are not standard namedtuples and do not provide the same API (for
 * example, there is no `_replace()` method).
 * https://docs.python.org/3/c-api/tuple.html
 */
onamedtupletype::onamedtupletype(const std::string& cls_name,
                                 const std::string& cls_doc,
                                 const std::vector<field> fields)
{
  auto itemgetter = py::oobj::import("operator", "itemgetter");
  auto namedtuple = py::oobj::import("collections", "namedtuple");
  auto property   = py::oobj::import("builtins", "property");

  // Create a namedtuple type from the supplied fields
  nfields = fields.size();
  py::olist argnames(nfields);
  for (size_t i = 0; i < nfields; ++i) {
    argnames.set(i, py::ostring(fields[i].name));
  }

  auto type = namedtuple.call({py::ostring(cls_name), argnames});
  auto res = std::move(type).release();

  // Set namedtuple doc
  if (!cls_doc.empty()) {
    py::ostring docstr = py::ostring(cls_doc);
    PyObject_SetAttrString(res, "__doc__",
                           docstr.to_borrowed_ref());
  }

  // Set field docs
  py::otuple args_prop(4);
  py::otuple args_itemgetter(1);
  args_prop.set(1, py::None());
  args_prop.set(2, py::None());
  for (size_t i = 0; i < nfields; ++i) {
    if (fields[i].doc.empty()) continue;
    args_itemgetter.set(0, py::oint(i));
    args_prop.set(0, itemgetter.call(args_itemgetter));
    args_prop.set(3, py::ostring(fields[i].doc));
    py::oobj prop = property.call(args_prop);
    PyObject_SetAttrString(res, fields[i].name.c_str(),
                           prop.to_borrowed_ref());
  }

  // Convert `PyObject*` to `PyTypeObject*`, so that it can be passed to
  // `py::onamedtuple` constructor
  v = reinterpret_cast<PyTypeObject*>(res);
}


onamedtupletype::onamedtupletype(const onamedtupletype& o) {
  v = o.v;
  nfields = o.nfields;
  Py_XINCREF(v);
}


onamedtupletype::onamedtupletype(onamedtupletype&& o) {
  v = o.v;
  nfields = o.nfields;
  o.v = nullptr;
  o.nfields = 0;
}


onamedtupletype::~onamedtupletype() {
  Py_XDECREF(v);
}



//------------------------------------------------------------------------------
// onamedtuple
//------------------------------------------------------------------------------

onamedtuple::onamedtuple(const onamedtupletype& type) {
  v = PyTuple_New(static_cast<Py_ssize_t>(type.nfields));
  if (!v) throw PyError();

  // Replace `Py_TYPE(v)`, that is a standard Python tuple,
  // with `type.v`, that is a named tuple type. Note, that
  // there is no need to call `Py_DECREF` on `Py_TYPE(v)`,
  // because tuple is a built-in type.
  Py_TYPE(v) = type.v;
  Py_INCREF(type.v);
}


}  // namespace py
