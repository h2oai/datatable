//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/obj.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors/destructors
//------------------------------------------------------------------------------

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

oobj::~oobj() {
  Py_XDECREF(obj);
}



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

bool _obj::is_none() const     { return (obj == Py_None); }
bool _obj::is_ellipsis() const { return (obj == Py_Ellipsis); }
bool _obj::is_bool() const     { return (obj == Py_True || obj == Py_False); }
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

int32_t _obj::to_int32(const error_manager& em) const {
  if (!is_int()) {
    throw em.error_not_integer(obj);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(obj, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    throw em.error_int32_overflow(obj);
  }
  return res;
}


int64_t _obj::to_int64(const error_manager& em) const {
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



//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error _obj::error_manager::error_not_integer(PyObject* o) const {
  return TypeError() << "Expected an integer, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_string(PyObject* o) const {
  return TypeError() << "Expected a string, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_int32_overflow(PyObject* o) const {
  return ValueError() << "Value is too large to fit in an int32: " << o;
}

Error _obj::error_manager::error_int64_overflow(PyObject* o) const {
  return ValueError() << "Value is too large to fit in an int64: " << o;
}


}  // namespace py
