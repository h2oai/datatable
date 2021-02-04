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


py::oobj PyType::make(Type type) {
  py::oobj res = py::robj(reinterpret_cast<PyObject*>(&PyType::type)).call();
  PyType* typed = reinterpret_cast<PyType*>(res.to_borrowed_ref());
  typed->type_ = std::move(type);
  return res;
}


static py::PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);


void PyType::m__init__(const py::PKArgs&) {}


void PyType::m__dealloc__() {
  type_ = Type();
}


py::oobj PyType::m__repr__() const {
  return py::ostring("Type." + type_.to_string());
}


py::oobj PyType::m__compare__(py::robj x, py::robj y, int op) {
  if (x.is_type() && y.is_type()) {
    dt::Type xtype = x.to_type();
    dt::Type ytype = y.to_type();
    if (op == Py_EQ) return py::obool(xtype == ytype);
    if (op == Py_NE) return py::obool(!(xtype == ytype));
  }
  return py::False();
}



static const char* doc_Type =
R"(
Type of data in a column.
)";

void PyType::impl_init_type(py::XTypeMaker& xt) {
  xt.set_class_name("datatable.Type");
  xt.set_class_doc(doc_Type);
  xt.set_subclassable(false);
  xt.set_meta_class(&PyTypeMeta::type);
  xt.add(CONSTRUCTOR(&PyType::m__init__, args___init__));
  xt.add(DESTRUCTOR(&PyType::m__dealloc__));
  xt.add(METHOD__REPR__(&PyType::m__repr__));
  xt.add(METHOD__CMP__(&PyType::m__compare__));
}



//------------------------------------------------------------------------------
// PyTypeMeta
//------------------------------------------------------------------------------

static const char* doc_TypeMeta = "Metaclass for dt.Type";

static py::PKArgs args_meta___init__(0, 0, 0, false, false, {}, "__init__", nullptr);

void PyTypeMeta::m__init__(const py::PKArgs&) {}
void PyTypeMeta::m__dealloc__() {}


// `typeMetaDict` cannot be simple `py::odict`, because otherwise it will
// get GCd after python shutdown, which will cause a segfault.
static py::odict* typeMetaDict = nullptr;
static void init_typemeta_dict() {
  typeMetaDict = new py::odict();
  typeMetaDict->set(py::ostring("void"),    PyType::make(Type::void0()));
  typeMetaDict->set(py::ostring("bool8"),   PyType::make(Type::bool8()));
  typeMetaDict->set(py::ostring("int8"),    PyType::make(Type::int8()));
  typeMetaDict->set(py::ostring("int16"),   PyType::make(Type::int16()));
  typeMetaDict->set(py::ostring("int32"),   PyType::make(Type::int32()));
  typeMetaDict->set(py::ostring("int64"),   PyType::make(Type::int64()));
  typeMetaDict->set(py::ostring("float32"), PyType::make(Type::float32()));
  typeMetaDict->set(py::ostring("float64"), PyType::make(Type::float64()));
  typeMetaDict->set(py::ostring("str32"),   PyType::make(Type::str32()));
  typeMetaDict->set(py::ostring("str64"),   PyType::make(Type::str64()));
  typeMetaDict->set(py::ostring("obj64"),   PyType::make(Type::obj64()));
}

py::oobj PyTypeMeta::m__getattr__(py::robj attr) {
  if (!typeMetaDict) init_typemeta_dict();
  auto res = typeMetaDict->get(attr);
  if (res) return res;

  std::string attr_str = attr.to_string();
  if (attr_str == "__dict__") {
    return *typeMetaDict;
  }
  if (py::is_python_system_attr(CString(attr_str))) {
    return py::oobj::from_new_reference(
        PyObject_GenericGetAttr(
          reinterpret_cast<PyObject*>(this),
          attr.to_borrowed_ref()
        ));
  }
  throw AttributeError() << "Type `" << attr_str << "` does not exist";
}


py::oobj PyTypeMeta::m__repr__() const {
  return py::ostring("<class 'datatable._TypeMeta'>");
}

void PyTypeMeta::impl_init_type(py::XTypeMaker& xt) {
  xt.set_class_name("datatable._TypeMeta");
  xt.set_base_class(&PyType_Type);
  xt.set_class_doc(doc_TypeMeta);
  xt.set_subclassable(false);
  xt.add(CONSTRUCTOR(&PyTypeMeta::m__init__, args_meta___init__));
  xt.add(DESTRUCTOR(&PyTypeMeta::m__dealloc__));
  xt.add(METHOD__REPR__(&PyTypeMeta::m__repr__));
  xt.add(METHOD__GETATTR__(&PyTypeMeta::m__getattr__));
}



}
