//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include <iostream>
#include "types/py_type.h"
#include "python/_all.h"
namespace dt {

static PyTypeObject* pythonType = nullptr;

py::oobj PyType::make(Type type) {
  xassert(pythonType);
  py::oobj res = py::robj(reinterpret_cast<PyObject*>(pythonType)).call();
  PyType* typed = reinterpret_cast<PyType*>(res.to_borrowed_ref());
  typed->type_ = std::move(type);
  return res;
}

bool PyType::check(PyObject* v) {
  if (!v) return false;
  auto typeptr = reinterpret_cast<PyObject*>(pythonType);
  int ret = PyObject_IsInstance(v, typeptr);
  if (ret == -1) PyErr_Clear();
  return (ret == 1);
}

PyType* PyType::cast_from(py::robj obj) {
  PyObject* v = obj.to_borrowed_ref();
  return PyType::check(v)? reinterpret_cast<PyType*>(v) : nullptr;
}


static py::PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);

void PyType::m__init__(const py::PKArgs&) {
}


void PyType::m__dealloc__() {
  type_ = Type();
}


py::oobj PyType::m__repr__() const {
  return py::ostring("Type." + type_.to_string());
}


py::oobj PyType::m__compare__(py::robj x, py::robj y, int op) {
  if (x.is_type() && y.is_type()) {
    dt::Type xtype = reinterpret_cast<PyType*>(x.to_borrowed_ref())->type_;
    dt::Type ytype = reinterpret_cast<PyType*>(y.to_borrowed_ref())->type_;
    if (op == Py_EQ) return py::obool(xtype == ytype);
    if (op == Py_NE) return py::obool(!(xtype == ytype));
  }
  return py::False();
}


static const char* doc_Type =
R"(
Type of data in a column.
)";

void PyType::init() {
  py::oobj pydtType = py::oobj::import("datatable", "Type");
  pythonType = reinterpret_cast<PyTypeObject*>(pydtType.to_borrowed_ref());
  xassert(pythonType->tp_basicsize == sizeof(PyType) - sizeof(dt::Type));
  pythonType->tp_basicsize = sizeof(PyType);

  py::XTypeMaker xt(pythonType, 0);
  xt.add(CONSTRUCTOR(&PyType::m__init__, args___init__));
  xt.add(DESTRUCTOR(&PyType::m__dealloc__));
  xt.add(METHOD__REPR__(&PyType::m__repr__));
  xt.add(METHOD__CMP__(&PyType::m__compare__));

  pydtType.set_attr("void",    PyType::make(Type::void0()));
  pydtType.set_attr("bool8",   PyType::make(Type::bool8()));
  pydtType.set_attr("int8",    PyType::make(Type::int8()));
  pydtType.set_attr("int16",   PyType::make(Type::int16()));
  pydtType.set_attr("int32",   PyType::make(Type::int32()));
  pydtType.set_attr("int64",   PyType::make(Type::int64()));
  pydtType.set_attr("float32", PyType::make(Type::float32()));
  pydtType.set_attr("float64", PyType::make(Type::float64()));
  pydtType.set_attr("str32",   PyType::make(Type::str32()));
  pydtType.set_attr("str64",   PyType::make(Type::str64()));
  pydtType.set_attr("obj64",   PyType::make(Type::obj64()));

  // Python ignores tp_doc on heap-allocated types
  pydtType.set_attr("__doc__", py::ostring(doc_Type));
}



}
