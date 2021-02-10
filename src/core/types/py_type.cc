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

// Pointer to the `class Type` that was created from python.
static PyObject* pythonType = nullptr;

py::oobj PyType::make(Type type) {
  xassert(pythonType);
  py::oobj res = py::robj(pythonType).call();
  PyType* typed = reinterpret_cast<PyType*>(res.to_borrowed_ref());
  typed->type_ = std::move(type);
  return res;
}



//------------------------------------------------------------------------------
// Initialization
//------------------------------------------------------------------------------

static py::odict* src_store = nullptr;
static void init_src_store_basic() {
  if (src_store) return;
  src_store = new py::odict();

  auto tVoid = PyType::make(Type::void0());
  src_store->set(py::ostring("void"), tVoid);
  src_store->set(py::ostring("V"), tVoid);
  src_store->set(py::None(), tVoid);

  auto tBool = PyType::make(Type::bool8());
  src_store->set(py::ostring("bool8"), tBool);
  src_store->set(py::ostring("boolean"), tBool);
  src_store->set(py::ostring("bool"), tBool);
  src_store->set(py::oobj(&PyBool_Type), tBool);

  src_store->set(py::ostring("int8"),  PyType::make(Type::int8()));
  src_store->set(py::ostring("int16"), PyType::make(Type::int16()));
  src_store->set(py::ostring("int32"), PyType::make(Type::int32()));

  auto tInt64 = PyType::make(Type::int64());
  src_store->set(py::ostring("int64"), tInt64);
  src_store->set(py::ostring("integer"), tInt64);
  src_store->set(py::ostring("int"), tInt64);
  src_store->set(py::oobj(&PyLong_Type), tInt64);

  auto tFloat32 = PyType::make(Type::float32());
  src_store->set(py::ostring("float32"), tFloat32);
  src_store->set(py::ostring("float"), tFloat32);

  auto tFloat64 = PyType::make(Type::float64());
  src_store->set(py::ostring("float64"), tFloat64);
  src_store->set(py::ostring("double"), tFloat64);
  src_store->set(py::oobj(&PyFloat_Type), tFloat64);

  auto tStr32 = PyType::make(Type::str32());
  src_store->set(py::ostring("str32"), tStr32);
  src_store->set(py::ostring("<U"), tStr32);
  src_store->set(py::ostring("str"), tStr32);
  src_store->set(py::ostring("string"), tStr32);
  src_store->set(py::oobj(&PyUnicode_Type), tStr32);

  src_store->set(py::ostring("str64"), PyType::make(Type::str64()));

  auto tObj64 = PyType::make(Type::obj64());
  src_store->set(py::ostring("obj64"), tObj64);
  src_store->set(py::ostring("obj"), tObj64);
  src_store->set(py::ostring("object"), tObj64);
  src_store->set(py::oobj(&PyBaseObject_Type), tObj64);
}


static bool stypes_imported = false;
static void init_src_store_from_stypes() {
  if (stypes_imported) return;
  stypes_imported = true;
  auto stype = py::oobj::import("datatable", "stype");

  src_store->set(stype.get_attr("void"), PyType::make(Type::void0()));
  src_store->set(stype.get_attr("bool8"), PyType::make(Type::bool8()));
  src_store->set(stype.get_attr("int8"), PyType::make(Type::int8()));
  src_store->set(stype.get_attr("int16"), PyType::make(Type::int16()));
  src_store->set(stype.get_attr("int32"), PyType::make(Type::int32()));
  src_store->set(stype.get_attr("int64"), PyType::make(Type::int64()));
  src_store->set(stype.get_attr("float32"), PyType::make(Type::float32()));
  src_store->set(stype.get_attr("float64"), PyType::make(Type::float64()));
  src_store->set(stype.get_attr("str32"), PyType::make(Type::str32()));
  src_store->set(stype.get_attr("str64"), PyType::make(Type::str64()));
  src_store->set(stype.get_attr("obj64"), PyType::make(Type::obj64()));
}


static bool numpy_types_imported = false;
static void init_src_store_from_numpy() {
  if (numpy_types_imported) return;
  auto np = py::get_module("numpy");
  if (!np) return;
  numpy_types_imported = true;
  auto dtype = np.get_attr("dtype");

  auto tVoid = PyType::make(Type::void0());
  src_store->set(np.get_attr("void"), tVoid);
  src_store->set(dtype.call({py::ostring("void")}), tVoid);

  auto tBool = PyType::make(Type::bool8());
  src_store->set(np.get_attr("bool_"), tBool);
  src_store->set(dtype.call({py::ostring("bool")}), tBool);

  auto tInt8 = PyType::make(Type::int8());
  src_store->set(np.get_attr("int8"), tInt8);
  src_store->set(dtype.call({py::ostring("int8")}), tInt8);

  auto tInt16 = PyType::make(Type::int16());
  src_store->set(np.get_attr("int16"), tInt16);
  src_store->set(dtype.call({py::ostring("int16")}), tInt16);

  auto tInt32 = PyType::make(Type::int32());
  src_store->set(np.get_attr("int32"), tInt32);
  src_store->set(dtype.call({py::ostring("int32")}), tInt32);

  auto tInt64 = PyType::make(Type::int64());
  src_store->set(np.get_attr("int64"), tInt64);
  src_store->set(dtype.call({py::ostring("int64")}), tInt64);

  auto tFloat32 = PyType::make(Type::float32());
  src_store->set(np.get_attr("float16"), tFloat32);
  src_store->set(np.get_attr("float32"), tFloat32);
  src_store->set(dtype.call({py::ostring("float16")}), tFloat32);
  src_store->set(dtype.call({py::ostring("float32")}), tFloat32);

  auto tFloat64 = PyType::make(Type::float64());
  src_store->set(np.get_attr("float64"), tFloat64);
  src_store->set(dtype.call({py::ostring("float64")}), tFloat64);

  auto tStr32 = PyType::make(Type::str32());
  src_store->set(np.get_attr("str_"), tStr32);
  src_store->set(dtype.call({py::ostring("str")}), tStr32);
}


static py::PKArgs args___init__(
    1, 0, 0, false, false, {"src"}, "__init__", nullptr);

void PyType::m__init__(const py::PKArgs& args) {
  auto src = args[0].to_oobj();
  if (!src) return;
  for (int i = 0; i < 3; i++) {
    if (i == 0) init_src_store_basic();
    if (i == 1) init_src_store_from_stypes();
    if (i == 2) init_src_store_from_numpy();
    auto stored_type = src_store->get(src);
    if (stored_type) {
      type_ = reinterpret_cast<PyType*>(stored_type.to_borrowed_ref())->type_;
      return;
    }
  }
  if (src.is_type()) {   // make Type objects from other Type objects
    type_ = reinterpret_cast<PyType*>(src.to_borrowed_ref())->type_;
    return;
  }
  throw ValueError() << "Cannot create Type object from "
                     << src.safe_repr().to_string();
}


void PyType::m__dealloc__() {
  type_ = Type();
}



//------------------------------------------------------------------------------
// Basic properties
//------------------------------------------------------------------------------

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


static py::GSArgs args_get_name("name", "The name of the type");

py::oobj PyType::get_name() const {
  return py::ostring(type_.to_string());
}




//------------------------------------------------------------------------------
// Class declaration
//------------------------------------------------------------------------------

static const char* doc_Type =
R"(
.. x-version-added:: 1.0.0

Type of data stored in a single column of a Frame.

The type describes both the logical meaning of the data (i.e. an integer,
a floating point number, a string, etc.), as well as storage requirement
of that data (the number of bits per element). Some types may carry
additional properties, such as a timezone  or precision.

.. note::

    This property replaces previous :class:`dt.stype` and :class:`dt.ltype`.
)";

void PyType::impl_init_type(py::XTypeMaker& xt) {
  xt.set_class_name("datatable.Type");
  xt.set_class_doc(doc_Type);
  xt.add(CONSTRUCTOR(&PyType::m__init__, args___init__));
  xt.add(DESTRUCTOR(&PyType::m__dealloc__));
  xt.add(METHOD__REPR__(&PyType::m__repr__));
  xt.add(METHOD__CMP__(&PyType::m__compare__));
  // xt.add(GETTER(&PyType::get_name, args_get_name));
  pythonType = xt.get_type_object();

  xt.add_attr("void",    PyType::make(Type::void0()));
  xt.add_attr("bool8",   PyType::make(Type::bool8()));
  xt.add_attr("int8",    PyType::make(Type::int8()));
  xt.add_attr("int16",   PyType::make(Type::int16()));
  xt.add_attr("int32",   PyType::make(Type::int32()));
  xt.add_attr("int64",   PyType::make(Type::int64()));
  xt.add_attr("float32", PyType::make(Type::float32()));
  xt.add_attr("float64", PyType::make(Type::float64()));
  xt.add_attr("str32",   PyType::make(Type::str32()));
  xt.add_attr("str64",   PyType::make(Type::str64()));
  xt.add_attr("obj64",   PyType::make(Type::obj64()));
}



}
