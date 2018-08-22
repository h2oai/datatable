//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/obj.h"
#include <cstdint>         // INT32_MAX
#include "py_column.h"
#include "py_datatable.h"
#include "py_groupby.h"
#include "py_rowindex.h"
#include "python/int.h"
#include "python/float.h"
#include "python/list.h"
#include "python/string.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors/destructors
//------------------------------------------------------------------------------
_obj::error_manager _obj::_em0;


obj::obj(PyObject* p) {
  v = p;
}

obj::obj(const obj& other) {
  v = other.v;
}

obj::obj(const oobj& other) {
  v = other.v;
}

obj& obj::operator=(const obj& other) {
  v = other.v;
  return *this;
}


oobj::oobj() {
  v = nullptr;
}

oobj::oobj(PyObject* p) {
  v = p;
  Py_INCREF(p);
}

oobj::oobj(const oobj& other) : oobj(other.v) {}
oobj::oobj(const obj& other) : oobj(other.v) {}

oobj::oobj(oobj&& other) {
  v = other.v;
  other.v = nullptr;
}

oobj::oobj(oInt&& other) {
  v = other.obj;
  other.obj = nullptr;;
}

oobj::oobj(oFloat&& other) {
  v = other.obj;
  other.obj = nullptr;;
}

oobj::oobj(ostring&& other) {
  v = other.obj;
  other.obj = nullptr;;
}


oobj& oobj::operator=(const oobj& other) {
  Py_XDECREF(v);
  v = other.v;
  Py_XINCREF(v);
  return *this;
}

oobj& oobj::operator=(oobj&& other) {
  Py_XDECREF(v);
  v = other.v;
  other.v = nullptr;
  return *this;
}

oobj oobj::from_new_reference(PyObject* p) {
  oobj res;
  res.v = p;
  return res;
}

oobj::~oobj() {
  Py_XDECREF(v);
}



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

bool _obj::is_undefined()     const noexcept { return (v == nullptr);}
bool _obj::is_none()          const noexcept { return (v == Py_None); }
bool _obj::is_ellipsis()      const noexcept { return (v == Py_Ellipsis); }
bool _obj::is_true()          const noexcept { return (v == Py_True); }
bool _obj::is_false()         const noexcept { return (v == Py_False); }
bool _obj::is_bool()          const noexcept { return is_true() || is_false(); }
bool _obj::is_numeric()       const noexcept { return is_float() || is_int(); }
bool _obj::is_list_or_tuple() const noexcept { return is_list() || is_tuple(); }
bool _obj::is_int()           const noexcept { return v && PyLong_Check(v) && !is_bool(); }
bool _obj::is_float()         const noexcept { return v && PyFloat_Check(v); }
bool _obj::is_string()        const noexcept { return v && PyUnicode_Check(v); }
bool _obj::is_list()          const noexcept { return v && PyList_Check(v); }
bool _obj::is_tuple()         const noexcept { return v && PyTuple_Check(v); }
bool _obj::is_dict()          const noexcept { return v && PyDict_Check(v); }
bool _obj::is_buffer()        const noexcept { return v && PyObject_CheckBuffer(v); }
bool _obj::is_range()         const noexcept { return v && PyRange_Check(v); }



//------------------------------------------------------------------------------
// Bool conversions
//------------------------------------------------------------------------------

int8_t _obj::to_bool(const error_manager& em) const {
  if (v == Py_None) return GETNA<int8_t>();
  if (v == Py_True) return 1;
  if (v == Py_False) return 0;
  if (PyLong_CheckExact(v)) {
    int overflow;
    long x = PyLong_AsLongAndOverflow(v, &overflow);
    if (x == 0 || x == 1) return static_cast<int8_t>(x);
    // In all other cases, including when an overflow occurs -- fall through
  }
  throw em.error_not_boolean(v);
}

int8_t _obj::to_bool_strict(const error_manager& em) const {
  if (v == Py_True) return 1;
  if (v == Py_False) return 0;
  throw em.error_not_boolean(v);
}

int8_t _obj::to_bool_force(const error_manager&) const noexcept {
  if (v == Py_None) return GETNA<int8_t>();
  int r = PyObject_IsTrue(v);
  if (r == -1) {
    PyErr_Clear();
    return GETNA<int8_t>();
  }
  return static_cast<int8_t>(r);
}



//------------------------------------------------------------------------------
// Integer conversions
//------------------------------------------------------------------------------

int32_t _obj::to_int32(const error_manager& em) const {
  constexpr int32_t MAX = std::numeric_limits<int32_t>::max();
  if (is_none()) return GETNA<int32_t>();
  if (!PyLong_Check(v)) {
    throw em.error_not_integer(v);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(v, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    res = (overflow == 1 || value > res) ? MAX : -MAX;
  }
  return res;
}


int32_t _obj::to_int32_strict(const error_manager& em) const {
  if (!PyLong_Check(v) || is_bool()) {
    throw em.error_not_integer(v);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(v, &overflow);
  int32_t res = static_cast<int32_t>(value);
  if (overflow || value != static_cast<long>(res)) {
    throw em.error_int32_overflow(v);
  }
  return res;
}


int64_t _obj::to_int64(const error_manager& em) const {
  constexpr int64_t MAX = std::numeric_limits<int64_t>::max();
  if (is_none()) return GETNA<int64_t>();
  if (!PyLong_Check(v)) {
    throw em.error_not_integer(v);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(v, &overflow);
  int64_t res = static_cast<int64_t>(value);
  if (overflow ) {
    res = overflow == 1 ? MAX : -MAX;
  }
  return res;
}


int64_t _obj::to_int64_strict(const error_manager& em) const {
  if (!PyLong_Check(v) || is_bool()) {
    throw em.error_not_integer(v);
  }
  int overflow;
  long value = PyLong_AsLongAndOverflow(v, &overflow);
  if (overflow) {
    throw em.error_int64_overflow(v);
  }
  return value;
}


py::Int _obj::to_pyint(const error_manager& em) const {
  if (PyLong_Check(v) || v == Py_None) {
    return py::Int(v);
  }
  throw em.error_not_integer(v);
}


py::oInt _obj::to_pyint_force(const error_manager&) const noexcept {
  if (PyLong_Check(v) || v == Py_None) {
    return py::oInt(v);
  }
  PyObject* num = PyNumber_Long(v);  // new ref
  if (!num) {
    PyErr_Clear();
    num = nullptr;
  }
  return py::oInt::_from_pyobject_no_checks(num);
}



//------------------------------------------------------------------------------
// Float conversions
//------------------------------------------------------------------------------

double _obj::to_double(const error_manager& em) const {
  if (PyFloat_Check(v)) return PyFloat_AsDouble(v);
  if (v == Py_None) return GETNA<double>();
  if (PyLong_Check(v)) {
    double res = PyLong_AsDouble(v);
    if (res == -1 && PyErr_Occurred()) {
      throw em.error_double_overflow(v);
    }
    return res;
  }
  throw em.error_not_double(v);
}


oFloat _obj::to_pyfloat_force(const error_manager&) const noexcept {
  if (PyFloat_Check(v) || v == Py_None) {
    return py::oFloat(v);
  }
  PyObject* num = PyNumber_Float(v);  // new ref
  if (!num) {
    PyErr_Clear();
    num = nullptr;
  }
  return py::oFloat::_from_pyobject_no_checks(num);
}




//------------------------------------------------------------------------------
// String conversions
//------------------------------------------------------------------------------

CString _obj::to_cstring(const error_manager& em) const {
  Py_ssize_t str_size;
  const char* str;

  if (PyUnicode_Check(v)) {
    str = PyUnicode_AsUTF8AndSize(v, &str_size);
    if (!str) throw PyError();  // e.g. MemoryError
  }
  else if (PyBytes_Check(v)) {
    str_size = PyBytes_Size(v);
    str = PyBytes_AsString(v);
  }
  else if (v == Py_None) {
    str_size = 0;
    str = nullptr;
  }
  else {
    throw em.error_not_string(v);
  }
  return CString { str, static_cast<int64_t>(str_size) };
}


std::string _obj::to_string(const error_manager& em) const {
  CString cs = to_cstring(em);
  return cs.ch? std::string(cs.ch, static_cast<size_t>(cs.size)) :
                std::string();
}


py::ostring _obj::to_pystring_force(const error_manager&) const noexcept {
  if (PyUnicode_Check(v) || v == Py_None) {
    return py::ostring(v);
  }
  PyObject* w = PyObject_Str(v);
  if (!w) {
    PyErr_Clear();
    w = nullptr;
  }
  return py::ostring(w);
}



//------------------------------------------------------------------------------
// List conversions
//------------------------------------------------------------------------------

py::list _obj::to_pylist(const error_manager& em) const {
  if (is_none()) return py::list();
  if (is_list() || is_tuple()) {
    return py::list(v);
  }
  throw em.error_not_list(v);
}


char** _obj::to_cstringlist(const error_manager&) const {
  if (v == Py_None) {
    return nullptr;
  }
  if (PyList_Check(v) || PyTuple_Check(v)) {
    bool islist = PyList_Check(v);
    Py_ssize_t count = Py_SIZE(v);
    char** res = nullptr;
    try {
      res = new char*[count + 1];
      for (Py_ssize_t i = 0; i <= count; ++i) res[i] = nullptr;
      for (Py_ssize_t i = 0; i < count; ++i) {
        PyObject* item = islist? PyList_GET_ITEM(v, i)
                               : PyTuple_GET_ITEM(v, i);
        if (PyUnicode_Check(item)) {
          PyObject* y = PyUnicode_AsEncodedString(item, "utf-8", "strict");
          if (!y) throw PyError();
          size_t len = static_cast<size_t>(PyBytes_Size(y));
          res[i] = new char[len + 1];
          memcpy(res[i], PyBytes_AsString(y), len + 1);
          Py_DECREF(y);
        } else
        if (PyBytes_Check(item)) {
          size_t len = static_cast<size_t>(PyBytes_Size(item));
          res[i] = new char[len + 1];
          memcpy(res[i], PyBytes_AsString(item), len + 1);
        } else {
          throw TypeError() << "Item " << i << " in the list is not a string: "
                            << item << " (" << PyObject_Type(item) << ")";
        }
      }
      return res;
    } catch (...) {
      // Clean-up `res` before re-throwing the exception
      for (Py_ssize_t i = 0; i < count; ++i) delete[] res[i];
      delete[] res;
      throw;
    }
  }
  throw TypeError() << "A list of strings is expected, got " << v;
}


strvec _obj::to_stringlist(const error_manager&) const {
  strvec res;
  if (PyList_Check(v) || PyTuple_Check(v)) {
    bool islist = PyList_Check(v);
    Py_ssize_t count = Py_SIZE(v);
    res.reserve(static_cast<size_t>(count));
    for (Py_ssize_t i = 0; i < count; ++i) {
      PyObject* item = islist? PyList_GET_ITEM(v, i)
                             : PyTuple_GET_ITEM(v, i);
      if (PyUnicode_Check(item)) {
        PyObject* y = PyUnicode_AsEncodedString(item, "utf-8", "strict");
        if (!y) throw PyError();
        res.push_back(PyBytes_AsString(y));
        Py_DECREF(y);
      } else
      if (PyBytes_Check(item)) {
        res.push_back(PyBytes_AsString(item));
      } else {
        throw TypeError() << "Item " << i << " in the list is not a string: "
                          << item  << " (" << PyObject_Type(item) << ")";
      }
    }
  } else if (v != Py_None) {
    throw TypeError() << "A list of strings is expected, got " << v;
  }
  return res;
}



//------------------------------------------------------------------------------
// Object conversions
//------------------------------------------------------------------------------

Groupby* _obj::to_groupby(const error_manager& em) const {
  if (v == Py_None) return nullptr;
  if (!PyObject_TypeCheck(v, &pygroupby::type)) {
    throw em.error_not_groupby(v);
  }
  return static_cast<pygroupby::obj*>(v)->ref;
}


RowIndex _obj::to_rowindex(const error_manager& em) const {
  if (v == Py_None) {
    return RowIndex();
  }
  if (!PyObject_TypeCheck(v, &pyrowindex::type)) {
    throw em.error_not_rowindex(v);
  }
  RowIndex* ref = static_cast<pyrowindex::obj*>(v)->ref;
  return ref ? RowIndex(*ref) : RowIndex();  // copy-constructor is called here
}


DataTable* _obj::to_frame(const error_manager& em) const {
  if (v == Py_None) return nullptr;
  if (!PyObject_TypeCheck(v, &pydatatable::type)) {
    throw em.error_not_frame(v);
  }
  return static_cast<pydatatable::obj*>(v)->ref;
}


Column* _obj::to_column(const error_manager& em) const {
  if (!PyObject_TypeCheck(v, &pycolumn::type)) {
    throw em.error_not_column(v);
  }
  return reinterpret_cast<pycolumn::obj*>(v)->ref;
}


PyObject* _obj::to_pyobject_newref() const noexcept {
  Py_INCREF(v);
  return v;
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

oobj _obj::get_attr(const char* attr) const {
  PyObject* res = PyObject_GetAttrString(v, attr);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}

oobj _obj::invoke(const char* fn, const char* format, ...) const {
  PyObject* callable = nullptr;
  PyObject* args = nullptr;
  PyObject* res = nullptr;
  do {
    callable = PyObject_GetAttrString(v, fn);  // new ref
    if (!callable) break;
    va_list va;
    va_start(va, format);
    args = Py_VaBuildValue(format, va);          // new ref
    va_end(va);
    if (!args) break;
    res = PyObject_CallObject(callable, args);   // new ref
  } while (0);
  Py_XDECREF(callable);
  Py_XDECREF(args);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}


PyTypeObject* _obj::typeobj() const noexcept {
  return Py_TYPE(v);
}


PyObject* oobj::release() {
  PyObject* t = v;
  v = nullptr;
  return t;
}


oobj None()  { return oobj(Py_None); }
oobj True()  { return oobj(Py_True); }
oobj False() { return oobj(Py_False); }



//------------------------------------------------------------------------------
// Error messages
//------------------------------------------------------------------------------

Error _obj::error_manager::error_not_boolean(PyObject* o) const {
  return TypeError() << "Expected a boolean, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_integer(PyObject* o) const {
  return TypeError() << "Expected an integer, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_double(PyObject* o) const {
  return TypeError() << "Expected a float, instead got " << Py_TYPE(o);
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

Error _obj::error_manager::error_not_column(PyObject* o) const {
  return TypeError() << "Expected a Column, instead got " << Py_TYPE(o);
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

Error _obj::error_manager::error_double_overflow(PyObject*) const {
  return ValueError() << "Value is too large to convert to double";
}


}  // namespace py
