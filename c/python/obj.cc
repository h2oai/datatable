//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/obj.h"
#include <cstdint>         // INT32_MAX
#include "py_datatable.h"
#include "py_groupby.h"
#include "py_rowindex.h"
#include "python/list.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors/destructors
//------------------------------------------------------------------------------
_obj::error_manager _obj::_em0;


bobj::bobj(PyObject* p) {
  obj = p;
}

bobj::bobj(const bobj& other) {
  obj = other.obj;
}

bobj& bobj::operator=(const bobj& other) {
  obj = other.obj;
  return *this;
}


oobj::oobj(PyObject* p) {
  obj = p;
  Py_INCREF(p);
}

oobj::oobj(const oobj& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

oobj::oobj(oobj&& other) {
  obj = other.obj;
  other.obj = nullptr;
}

oobj oobj::from_new_reference(PyObject* p) {
  oobj res;
  res.obj = p;
  return res;
}

oobj::~oobj() {
  Py_XDECREF(obj);
}



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

bool _obj::is_none() const     { return (obj == Py_None); }
bool _obj::is_ellipsis() const { return (obj == Py_Ellipsis); }
bool _obj::is_true() const     { return (obj == Py_True); }
bool _obj::is_false() const    { return (obj == Py_False); }
bool _obj::is_bool() const     { return is_true() || is_false(); }
bool _obj::is_int() const      { return PyLong_Check(obj) && !is_bool(); }
bool _obj::is_float() const    { return PyFloat_Check(obj); }
bool _obj::is_numeric() const  { return is_float() || is_int(); }
bool _obj::is_string() const   { return PyUnicode_Check(obj); }
bool _obj::is_list() const     { return PyList_Check(obj); }
bool _obj::is_tuple() const    { return PyTuple_Check(obj); }
bool _obj::is_dict() const     { return PyDict_Check(obj); }



//------------------------------------------------------------------------------
// Type conversions
//------------------------------------------------------------------------------

int8_t _obj::to_bool_strict(const error_manager& em) const {
  if (obj == Py_True) return 1;
  if (obj == Py_False) return 0;
  throw em.error_not_boolean(obj);
}

int8_t _obj::to_bool_force() const {
  if (obj == Py_None) return GETNA<int8_t>();
  int r = PyObject_IsTrue(obj);
  if (r == -1) {
    PyErr_Clear();
    return GETNA<int8_t>();
  }
  return static_cast<int8_t>(r);
}


template <int MODE>
int32_t _obj::_to_int32(const error_manager& em) const {
  constexpr int32_t MAX = std::numeric_limits<int32_t>::max();
  if (!is_int()) {
    throw em.error_not_integer(obj);
  }
  if (MODE == 3) { // mask
    unsigned long value = PyLong_AsUnsignedLongMask(obj);
    return static_cast<int32_t>(value);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(obj, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    if (MODE == 1)
      throw em.error_int32_overflow(obj);
    if (MODE == 2) {  // truncate
      if (overflow == 1 || value > res) res = MAX;
      else res = -MAX;
    }
  }
  return res;
}


int32_t _obj::to_int32_strict(const error_manager& em) const {
  return _to_int32<1>(em);
}

int32_t _obj::to_int32_truncate(const error_manager& em) const {
  return _to_int32<2>(em);
}

int32_t _obj::to_int32_mask(const error_manager& em) const {
  return _to_int32<1>(em);
}



int64_t _obj::to_int64_strict(const error_manager& em) const {
  if (!is_int()) {
    throw em.error_not_integer(obj);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(obj, &overflow);
  if (overflow) {
    throw em.error_int64_overflow(obj);
  }
  return value;
}


CString _obj::to_cstring(const error_manager& em) const {
  if (!is_string()) {
    throw em.error_not_string(obj);
  }
  Py_ssize_t str_size;
  const char* str = PyUnicode_AsUTF8AndSize(obj, &str_size);
  if (!str) {
    throw PyError();
  }
  return CString { str, static_cast<int64_t>(str_size) };
}


std::string _obj::to_string(const error_manager& em) const {
  CString cs = to_cstring(em);
  return std::string(cs.ch, static_cast<size_t>(cs.size));
}


PyObject* _obj::to_pyobject_newref() const {
  Py_INCREF(obj);
  return obj;
}


py::list _obj::to_list(const error_manager& em) const {
  if (!(is_list() || is_tuple())) {
    throw em.error_not_list(obj);
  }
  return py::list(obj);
}


Groupby* _obj::to_groupby(const error_manager& em) const {
  if (obj == Py_None) return nullptr;
  if (!PyObject_TypeCheck(obj, &pygroupby::type)) {
    throw em.error_not_groupby(obj);
  }
  return static_cast<pygroupby::obj*>(obj)->ref;
}


RowIndex _obj::to_rowindex(const error_manager& em) const {
  if (obj == Py_None) {
    return RowIndex();
  }
  if (!PyObject_TypeCheck(obj, &pyrowindex::type)) {
    throw em.error_not_rowindex(obj);
  }
  RowIndex* ref = static_cast<pyrowindex::obj*>(obj)->ref;
  return ref ? RowIndex(*ref) : RowIndex();  // copy-constructor is called here
}


DataTable* _obj::to_frame(const error_manager& em) const {
  if (obj == Py_None) return nullptr;
  if (!PyObject_TypeCheck(obj, &pydatatable::type)) {
    throw em.error_not_frame(obj);
  }
  return static_cast<pydatatable::obj*>(obj)->ref;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

oobj _obj::get_attr(const char* attr) const {
  PyObject* res = PyObject_GetAttrString(obj, attr);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}

PyObject* oobj::release() {
  PyObject* t = obj;
  obj = nullptr;
  return t;
}

oobj _obj::none() {
  return oobj(Py_None);
}



//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error _obj::error_manager::error_not_boolean(PyObject* o) const {
  return TypeError() << "Expected a boolean, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_integer(PyObject* o) const {
  return TypeError() << "Expected an integer, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_string(PyObject* o) const {
  return TypeError() << "Expected a string, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_groupby(PyObject* o) const {
  return TypeError() << "Expected a Groupby, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_rowindex(PyObject* o) const {
  return TypeError() << "Expected a RowIndex, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_frame(PyObject* o) const {
  return TypeError() << "Expected a Frame, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_list(PyObject* o) const {
  return TypeError() << "Expected a list or tuple, instead got "
      << Py_TYPE(o);
}

Error _obj::error_manager::error_int32_overflow(PyObject* o) const {
  return ValueError() << "Value is too large to fit in an int32: " << o;
}

Error _obj::error_manager::error_int64_overflow(PyObject* o) const {
  return ValueError() << "Value is too large to fit in an int64: " << o;
}


}  // namespace py
