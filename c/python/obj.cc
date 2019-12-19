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
#include <iostream>
#include <cstdint>         // INT32_MAX
#include "expr/py_by.h"       // py::oby
#include "expr/py_join.h"     // py::ojoin
#include "expr/py_sort.h"     // py::osort
#include "expr/py_update.h"   // py::oupdate
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/list.h"
#include "python/obj.h"
#include "python/string.h"

namespace py {
static PyObject* pandas_DataFrame_type = nullptr;
static PyObject* pandas_Series_type = nullptr;
static PyObject* numpy_Array_type = nullptr;
static PyObject* numpy_MaskedArray_type = nullptr;
static PyObject* numpy_int8 = nullptr;
static PyObject* numpy_int16 = nullptr;
static PyObject* numpy_int32 = nullptr;
static PyObject* numpy_int64 = nullptr;
static PyObject* numpy_float16 = nullptr;
static PyObject* numpy_float32 = nullptr;
static PyObject* numpy_float64 = nullptr;
static void init_pandas();
static void init_numpy();

// Set from datatablemodule.cc
PyObject* Expr_Type = nullptr;

// `_Py_static_string_init` invoked by the `_Py_IDENTIFIER` uses
// a designated initializer, that is not supported by the C++11 standard.
// Redefine `_Py_static_string_init` here to use a regular initializer.
#undef _Py_static_string_init
#define _Py_static_string_init(value) { NULL, value, NULL }
_Py_IDENTIFIER(stdin);
_Py_IDENTIFIER(stdout);
_Py_IDENTIFIER(write);



//------------------------------------------------------------------------------
// Constructors/destructors
//------------------------------------------------------------------------------
_obj::error_manager _obj::_em0;

robj::robj() {
  v = nullptr;
}

robj::robj(const PyObject* p) {
  v = const_cast<PyObject*>(p);
}

robj::robj(const Arg& arg) {
  v = arg.to_borrowed_ref();
}

robj::robj(const robj& other) {
  v = other.v;
}

robj::robj(const oobj& other) {
  v = other.v;
}

robj& robj::operator=(const robj& other) {
  v = other.v;
  return *this;
}

robj& robj::operator=(const _obj& other) {
  v = other.v;
  return *this;
}


oobj::oobj() {
  v = nullptr;
}

oobj::oobj(PyObject* p) {
  v = p;
  if (p) Py_INCREF(p);
}

oobj::oobj(const oobj& other) : oobj(other.v) {}
oobj::oobj(const robj& other) : oobj(other.v) {}

oobj::oobj(oobj&& other) {
  v = other.v;
  other.v = nullptr;
}



oobj& oobj::operator=(const oobj& other) {
  PyObject* t = v;
  v = other.v;
  Py_XINCREF(other.v);
  Py_XDECREF(t);
  return *this;
}

oobj& oobj::operator=(oobj&& other) {
  PyObject* t = v;
  v = other.v;
  other.v = nullptr;
  Py_XDECREF(t);
  return *this;
}

oobj oobj::from_new_reference(PyObject* p) {
  oobj res;
  res.v = p;
  return res;
}

oobj oobj::import(const char* mod) {
  PyObject* module = PyImport_ImportModule(mod);
  if (!module) {
    if (PyErr_ExceptionMatches(PyExc_ImportError)) {
      PyErr_Clear();
      throw ImportError() << "Module `" << mod << "` is not installed. "
                             "It is required for running this function";
    } else {
      throw PyError();
    }
  }
  return oobj::from_new_reference(module);
}

oobj oobj::import(const char* mod, const char* symbol) {
  auto mod_obj = oobj::from_new_reference(PyImport_ImportModule(mod));
  if (!mod_obj) throw PyError();
  return mod_obj.get_attr(symbol);
}

oobj oobj::import(const char* mod, const char* sym1, const char* sym2) {
  return import(mod, sym1).get_attr(sym2);
}

oobj::~oobj() {
  if (v) Py_DECREF(v);
}

oobj oobj::wrap(bool v)    { return py::obool(v); }
oobj oobj::wrap(int8_t v)  { return py::oint(v); }
oobj oobj::wrap(int16_t v) { return py::oint(v); }
oobj oobj::wrap(int32_t v) { return py::oint(v); }
oobj oobj::wrap(int64_t v) { return py::oint(v); }
oobj oobj::wrap(size_t v)  { return py::oint(v); }
oobj oobj::wrap(float v)   { return py::ofloat(v); }
oobj oobj::wrap(double v)  { return py::ofloat(v); }
oobj oobj::wrap(const CString& v) { return py::ostring(v); }
oobj oobj::wrap(const robj& v) { return py::oobj(v); }



//------------------------------------------------------------------------------
// Type checks
//------------------------------------------------------------------------------

_obj::operator bool() const noexcept { return (v != nullptr); }
bool _obj::operator==(const _obj& other) const noexcept { return v == other.v; }
bool _obj::operator!=(const _obj& other) const noexcept { return v != other.v; }

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
bool _obj::is_bytes()         const noexcept { return v && PyBytes_Check(v); }
bool _obj::is_type()          const noexcept { return v && PyType_Check(v); }
bool _obj::is_ltype()         const noexcept { return v && Py_TYPE(v) == py_ltype; }
bool _obj::is_stype()         const noexcept { return v && Py_TYPE(v) == py_stype; }
bool _obj::is_anytype()       const noexcept { return is_type() || is_stype() || is_ltype(); }
bool _obj::is_list()          const noexcept { return v && PyList_Check(v); }
bool _obj::is_tuple()         const noexcept { return v && PyTuple_Check(v); }
bool _obj::is_dict()          const noexcept { return v && PyDict_Check(v); }
bool _obj::is_buffer()        const noexcept { return v && PyObject_CheckBuffer(v); }
bool _obj::is_range()         const noexcept { return v && PyRange_Check(v); }
bool _obj::is_slice()         const noexcept { return v && PySlice_Check(v); }
bool _obj::is_generator()     const noexcept { return v && PyGen_Check(v); }

bool _obj::is_iterable() const noexcept {
  return v && (v->ob_type->tp_iter || PySequence_Check(v));
}

bool _obj::is_frame() const noexcept {
  return py::Frame::check(v);
}

bool _obj::is_join_node() const noexcept {
  return py::ojoin::check(v);
}

bool _obj::is_by_node() const noexcept {
  return py::oby::check(v);
}

bool _obj::is_sort_node() const noexcept {
  return py::osort::check(v);
}

bool _obj::is_update_node() const noexcept {
  return py::oupdate::check(v);
}

bool _obj::is_pandas_frame() const noexcept {
  if (!pandas_DataFrame_type) init_pandas();
  if (!v || !pandas_DataFrame_type) return false;
  return PyObject_IsInstance(v, pandas_DataFrame_type);
}

bool _obj::is_pandas_series() const noexcept {
  if (!pandas_Series_type) init_pandas();
  if (!v || !pandas_Series_type) return false;
  return PyObject_IsInstance(v, pandas_Series_type);
}

bool _obj::is_numpy_array() const noexcept {
  if (!numpy_Array_type) init_numpy();
  if (!v || !numpy_Array_type) return false;
  return PyObject_IsInstance(v, numpy_Array_type);
}

int _obj::is_numpy_int() const noexcept {
  if (!numpy_int64) init_numpy();
  if (!v || !numpy_int64) return 0;
  if (PyObject_IsInstance(v, numpy_int64)) return 8;
  if (PyObject_IsInstance(v, numpy_int32)) return 4;
  if (PyObject_IsInstance(v, numpy_int16)) return 2;
  if (PyObject_IsInstance(v, numpy_int8))  return 1;
  return 0;
}

int _obj::is_numpy_float() const noexcept {
  if (!numpy_int64) init_numpy();
  if (!v || !numpy_float64) return 0;
  if (PyObject_IsInstance(v, numpy_float64)) return 8;
  if (PyObject_IsInstance(v, numpy_float32)) return 4;
  if (PyObject_IsInstance(v, numpy_float16)) return 4;
  return 0;
}

bool _obj::is_numpy_marray() const noexcept {
  if (!numpy_MaskedArray_type) init_numpy();
  if (!v || !numpy_MaskedArray_type) return false;
  return PyObject_IsInstance(v, numpy_MaskedArray_type);
}

bool _obj::is_dtexpr() const noexcept {
  if (!Expr_Type || !v) return false;
  return PyObject_IsInstance(v, Expr_Type);
}



//------------------------------------------------------------------------------
// Value parsers
//------------------------------------------------------------------------------

template <typename T>
static inline bool _parse_none(PyObject* v, T* out) {
  if (v == Py_None) { *out = GETNA<T>(); return true; }
  return false;
}

bool _obj::parse_none(int8_t* out)  const { return _parse_none(v, out); }
bool _obj::parse_none(int16_t* out) const { return _parse_none(v, out); }
bool _obj::parse_none(int32_t* out) const { return _parse_none(v, out); }
bool _obj::parse_none(int64_t* out) const { return _parse_none(v, out); }
bool _obj::parse_none(float* out)   const { return _parse_none(v, out); }
bool _obj::parse_none(double* out)  const { return _parse_none(v, out); }



template <typename T>
static inline bool _parse_bool(PyObject* v, T* out) {
  if (v == Py_True)  { *out = 1; return true; }
  if (v == Py_False) { *out = 0; return true; }
  return false;
}

bool _obj::parse_bool(int8_t* out) const  { return _parse_bool(v, out); }
bool _obj::parse_bool(int16_t* out) const { return _parse_bool(v, out); }
bool _obj::parse_bool(int32_t* out) const { return _parse_bool(v, out); }
bool _obj::parse_bool(int64_t* out) const { return _parse_bool(v, out); }
bool _obj::parse_bool(double* out)  const { return _parse_bool(v, out); }



template <typename T>
static inline bool _parse_01(PyObject* v, T* out) {
  if (PyLong_CheckExact(v)) {
    int overflow;
    long x = PyLong_AsLongAndOverflow(v, &overflow);
    if (x == 0 || x == 1) {
      *out = static_cast<T>(x);
      return true;
    }
    // Fall-through for all other values of `x`, including overflow (x == -1)
  }
  return false;
}

bool _obj::parse_01(int8_t* out)  const { return _parse_01(v, out); }
bool _obj::parse_01(int16_t* out) const { return _parse_01(v, out); }




template <typename T>
bool _parse_int(PyObject* v, T* out) {
  static_assert(sizeof(T) <= sizeof(long), "Wrong size of T");
  if (PyLong_Check(v)) {
    int overflow;
    long value = PyLong_AsLongAndOverflow(v, &overflow);
    auto res = static_cast<T>(value);
    if (!overflow && value == res) {
      *out = res;
      return true;
    }
  }
  return false;
}

#if LONG_MAX != 9223372036854775807
  template <>
  bool _parse_int(PyObject* v, int64_t* out) {
    static_assert(sizeof(int64_t) <= sizeof(long long), "Wrong size of long long");
    if (PyLong_Check(v)) {
      int overflow;
      long long value = PyLong_AsLongLongAndOverflow(v, &overflow);
      auto res = static_cast<int64_t>(value);
      if (!overflow && value == res) {
        *out = res;
        return true;
      }
    }
    return false;
  }
#endif

bool _obj::parse_int(int8_t* out) const  { return _parse_int(v, out); }
bool _obj::parse_int(int16_t* out) const { return _parse_int(v, out); }
bool _obj::parse_int(int32_t* out) const { return _parse_int(v, out); }
bool _obj::parse_int(int64_t* out) const { return _parse_int(v, out); }

bool _obj::parse_int(double* out) const {
  if (PyLong_Check(v)) {
    double value = PyLong_AsDouble(v);
    if (value == -1.0 && PyErr_Occurred()) {
      int sign = _PyLong_Sign(v);
      value = sign > 0 ? std::numeric_limits<double>::infinity()
                       : -std::numeric_limits<double>::infinity();
      PyErr_Clear();
    }
    *out = value;
    return true;
  }
  return false;
}



template <typename T>
static bool _parse_npint(PyObject* v, T* out) {
  if (!numpy_int64) init_numpy();
  if (numpy_int64 && v) {
    if ((sizeof(T) >= 8 && PyObject_IsInstance(v, numpy_int64)) ||
        (sizeof(T) >= 4 && PyObject_IsInstance(v, numpy_int32)) ||
        (sizeof(T) >= 2 && PyObject_IsInstance(v, numpy_int16)) ||
        (sizeof(T) >= 1 && PyObject_IsInstance(v, numpy_int8)))
    {
      *out = static_cast<T>(PyNumber_AsSsize_t(v, nullptr));
      return true;
    }
  }
  return false;
}

bool _obj::parse_numpy_int(int8_t* out)  const { return _parse_npint(v, out); }
bool _obj::parse_numpy_int(int16_t* out) const { return _parse_npint(v, out); }
bool _obj::parse_numpy_int(int32_t* out) const { return _parse_npint(v, out); }
bool _obj::parse_numpy_int(int64_t* out) const { return _parse_npint(v, out); }


template <typename T>
static bool _parse_npfloat(PyObject* v, T* out) {
  if (!numpy_float64) init_numpy();
  if (numpy_float64 && v) {
    if ((sizeof(T) >= 8 && PyObject_IsInstance(v, numpy_float64)) ||
        (sizeof(T) >= 4 && PyObject_IsInstance(v, numpy_float32)) ||
        (sizeof(T) >= 2 && PyObject_IsInstance(v, numpy_float16)))
    {
      PyObject* num = PyNumber_Float(v);  // new ref
      if (num) {
        *out = static_cast<T>(PyFloat_AsDouble(num));
        Py_DECREF(num);
        return true;
      }
      PyErr_Clear();
    }
  }
  return false;
}

bool _obj::parse_numpy_float(float*  out) const { return _parse_npfloat(v, out); }
bool _obj::parse_numpy_float(double* out) const { return _parse_npfloat(v, out); }


bool _obj::parse_double(double* out) const {
  if (PyFloat_Check(v)) {
    *out = PyFloat_AsDouble(v);
    return true;
  }
  return false;
}




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
  if (v == Py_True) return 1;
  if (v == Py_False) return 0;
  int r = PyObject_IsTrue(v);
  if (r >= 0) return static_cast<int8_t>(r);
  PyErr_Clear();
  return GETNA<int8_t>();
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
  if (PyLong_Check(v)) {
    int overflow;
    long value = PyLong_AsLongAndOverflow(v, &overflow);
    int64_t res = static_cast<int64_t>(value);
    if (overflow ) {
      res = overflow == 1 ? MAX : -MAX;
    }
    return res;
  }
  if (PyNumber_Check(v)) {
    return static_cast<int64_t>(PyNumber_AsSsize_t(v, nullptr));
  }
  throw em.error_not_integer(v);
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


size_t _obj::to_size_t(const error_manager& em) const {
  int64_t res = to_int64_strict(em);
  if (res < 0) {
    throw em.error_int_negative(v);
  }
  return static_cast<size_t>(res);
}


py::oint _obj::to_pyint(const error_manager& em) const {
  if (v == Py_None) return py::oint();
  if (PyLong_Check(v)) return py::oint(robj(v));
  throw em.error_not_integer(v);
}


py::oint _obj::to_pyint_force(const error_manager&) const noexcept {
  if (v == Py_None) return py::oint();
  if (PyLong_Check(v)) return py::oint(robj(v));
  PyObject* num = PyNumber_Long(v);  // new ref
  if (!num) {
    PyErr_Clear();
    num = nullptr;
  }
  return py::oint(oobj::from_new_reference(num));
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
  if (PyNumber_Check(v)) {
    PyObject* num = PyNumber_Float(v); // new ref
    if (num) {
      double value = PyFloat_AsDouble(num);
      Py_DECREF(num);
      return value;
    }
    PyErr_Clear();
  }
  throw em.error_not_double(v);
}


py::ofloat _obj::to_pyfloat_force(const error_manager&) const noexcept {
  if (PyFloat_Check(v) || v == Py_None) {
    return py::ofloat(robj(v));
  }
  PyObject* num = PyNumber_Float(v);  // new ref
  if (!num) {
    PyErr_Clear();
    num = nullptr;
  }
  return py::ofloat(oobj::from_new_reference(num));
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

py::olist _obj::to_pylist(const error_manager& em) const {
  if (is_none()) return py::olist(nullptr);
  if (is_list() || is_tuple()) {
    return py::olist(v);
  }
  throw em.error_not_list(v);
}


py::otuple _obj::to_otuple(const error_manager& em) const {
  if (is_none()) return py::otuple();
  if (is_tuple()) return py::otuple(robj(v));
  throw em.error_not_list(v);
}

py::rtuple _obj::to_rtuple_lax() const {
  if (is_tuple()) return py::rtuple(robj(v));
  return py::rtuple(nullptr);
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
      res = new char*[count + 1]();
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
  if (!is_list_or_tuple()) {
    throw TypeError() << "A list of strings is expected, got " << v;
  }
  auto vlist = to_pylist();
  size_t count = vlist.size();

  strvec res;
  res.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    robj item = vlist[i];
    if (item.is_string() || item.is_bytes()) {
      res.push_back(item.to_string());
    } else {
      throw TypeError() << "Item " << i << " in the list is not a string: "
                        << item.typeobj();
    }
  }
  return res;
}



//------------------------------------------------------------------------------
// Object conversions
//------------------------------------------------------------------------------

DataTable* _obj::to_datatable(const error_manager& em) const {
  if (v == Py_None) return nullptr;
  if (is_frame()) {
    return static_cast<py::Frame*>(v)->get_datatable();
  }
  throw em.error_not_frame(v);
}



SType _obj::to_stype(const error_manager& em) const {
  int s = stype_from_pyobject(v);
  if (s == -1) {
    throw em.error_not_stype(v);
  }
  return static_cast<SType>(s);
}


py::ojoin _obj::to_ojoin_lax() const {
  if (is_join_node()) {
    return ojoin(robj(v));
  }
  return ojoin();
}


py::oby _obj::to_oby_lax() const {
  if (is_by_node()) {
    return oby(robj(v));
  }
  return oby();
}


py::osort _obj::to_osort_lax() const {
  return is_sort_node()? osort(robj(v)) : osort();
}


py::oupdate _obj::to_oupdate_lax() const {
  return is_update_node()? oupdate(robj(v)) : oupdate();
}


PyObject* _obj::to_pyobject_newref() const noexcept {
  Py_INCREF(v);
  return v;
}


py::odict _obj::to_pydict(const error_manager& em) const {
  if (is_none()) return py::odict();
  if (is_dict()) return py::odict(robj(v));
  throw em.error_not_dict(v);
}


py::rdict _obj::to_rdict(const error_manager& em) const {
  if (!is_dict()) throw em.error_not_dict(v);
  return py::rdict(robj(v));
}


py::orange _obj::to_orange(const error_manager& em) const {
  if (is_none()) return py::orange(nullptr);
  if (is_range()) return py::orange(v);
  throw em.error_not_range(v);
}


py::oiter _obj::to_oiter(const error_manager& em) const {
  if (is_none()) return py::oiter();
  if (is_iterable()) return py::oiter(v);
  throw em.error_not_iterable(v);
}


py::oslice _obj::to_oslice(const error_manager& em) const {
  if (is_none()) return py::oslice();
  if (is_slice()) return py::oslice(robj(v));
  throw em.error_not_slice(v);
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

oobj _obj::get_attr(const char* attr) const {
  PyObject* res = PyObject_GetAttrString(v, attr);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}

oobj _obj::get_attrx(const char* attr) const {
  PyObject* res = PyObject_GetAttrString(v, attr);
  if (!res) {
    PyErr_Clear();
    return oobj();
  }
  return oobj::from_new_reference(res);
}


bool _obj::has_attr(const char* attr) const {
  return PyObject_HasAttrString(v, attr);
}


oobj _obj::get_item(const py::_obj& key) const {
  // PyObject_GetItem returns a new reference, or NULL on failure
  PyObject* res = PyObject_GetItem(v, key.v);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}


oobj _obj::get_iter() const {
  // PyObject_GetIter(v)
  //   This is equivalent to the Python expression iter(o). It returns a new
  //   iterator for the object argument, or the object itself if the object is
  //   already an iterator. Raises TypeError and returns NULL if the object
  //   cannot be iterated.
  //     Return value: New reference.
  PyObject* iter = PyObject_GetIter(v);
  if (!iter) throw PyError();
  return oobj::from_new_reference(iter);
}


oobj _obj::invoke(const char* fn) const {
  oobj method = get_attr(fn);
  PyObject* res = PyObject_CallObject(method.v, nullptr);  // new ref
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}


oobj _obj::invoke(const char* fn, otuple&& args) const {
  oobj method = get_attr(fn);
  PyObject* res = PyObject_CallObject(method.v, args.v);  // new ref
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}

oobj _obj::invoke(const char* fn, const oobj& arg1) const {
  return invoke(fn, otuple(arg1));
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


oobj _obj::call() const {
  PyObject* res = PyObject_CallObject(v, nullptr);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}

oobj _obj::call(otuple args) const {
  PyObject* res = PyObject_CallObject(v, args.v);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}


oobj _obj::call(otuple args, odict kws) const {
  PyObject* res = PyObject_Call(v, args.v, kws.v);
  if (!res) throw PyError();
  return oobj::from_new_reference(res);
}


ostring _obj::str() const {
  return ostring::from_new_reference(PyObject_Str(v));
}


PyTypeObject* _obj::typeobj() const noexcept {
  return Py_TYPE(v);
}

std::string _obj::typestr() const {
  return std::string(Py_TYPE(v)->tp_name);
}


size_t _obj::get_sizeof() const {
  return _PySys_GetSizeOf(v);
}


PyObject* oobj::release() && {
  PyObject* t = v;
  v = nullptr;
  return t;
}


oobj get_module(const char* modname) {
  py::ostring pyname(modname);
  #if PY_VERSION_HEX >= 0x03070000
    // PyImport_GetModule(name)
    //   Return the already imported module with the given name. If the module
    //   has not been imported yet then returns NULL but does not set an error.
    //   Returns NULL and sets an error if the lookup failed.
    PyObject* res = PyImport_GetModule(pyname.v);
    if (!res && PyErr_Occurred()) PyErr_Clear();
    return oobj::from_new_reference(res);

  #else
    // PyImport_GetModuleDict() returns a borrowed ref
    oobj sys_modules(PyImport_GetModuleDict());
    if (sys_modules.v == nullptr) {
      PyErr_Clear();
      return oobj(nullptr);
    }
    if (PyDict_CheckExact(sys_modules.v)) {
      // PyDict_GetItem() returns a borowed ref, or NULL if the key is not
      // present (without setting an exception)
      return oobj(PyDict_GetItem(sys_modules.v, pyname.v));
    } else {
      // PyObject_GetItem() returns a new reference
      PyObject* res = PyObject_GetItem(sys_modules.v, pyname.v);
      if (!res) PyErr_Clear();
      return oobj::from_new_reference(res);
    }

  #endif
}


static void init_pandas() {
  py::oobj pd = get_module("pandas");
  if (pd) {
    pandas_DataFrame_type = pd.get_attr("DataFrame").release();
    pandas_Series_type    = pd.get_attr("Series").release();
  }
}

static void init_numpy() {
  py::oobj np = get_module("numpy");
  if (np) {
    numpy_Array_type = np.get_attr("ndarray").release();
    numpy_MaskedArray_type
      = np.get_attr("ma").get_attr("MaskedArray").release();
    numpy_int8 = np.get_attr("int8").release();
    numpy_int16 = np.get_attr("int16").release();
    numpy_int32 = np.get_attr("int32").release();
    numpy_int64 = np.get_attr("int64").release();
    numpy_float16 = np.get_attr("float16").release();
    numpy_float32 = np.get_attr("float32").release();
    numpy_float64 = np.get_attr("float64").release();
  }
}


oobj None()     { return oobj(Py_None); }
oobj True()     { return oobj(Py_True); }
oobj False()    { return oobj(Py_False); }
oobj Ellipsis() { return oobj(Py_Ellipsis); }
robj rnone()    { return robj(Py_None); }

robj rstdin() {
  return robj(
    #ifndef Py_LIMITED_API
      _PySys_GetObjectId(&PyId_stdin)  // borrowed ref
    #else
      PySys_GetObject("stdin")         // borrowed ref
    #endif
  );
}

robj rstdout() {
  return robj(
    #ifndef Py_LIMITED_API
      _PySys_GetObjectId(&PyId_stdout)  // borrowed ref
    #else
      PySys_GetObject("stdout")         // borrowed ref
    #endif
  );
}

void write_to_stdout(const std::string& str) {
  PyObject* py_stdout = rstdout().to_borrowed_ref();
  oobj writer;
  if (py_stdout && py_stdout != Py_None) {
    writer = oobj::from_new_reference(
      #ifndef Py_LIMITED_API
        _PyObject_GetAttrId(py_stdout, &PyId_write)  // new ref
      #else
        PyObject_GetAttrString(py_stdout, "write")   // new ref
      #endif
    );
    if (!writer) PyErr_Clear();
  }
  if (writer) {
    writer.call({ ostring(str) });
  }
  else {
    std::cout << str;
  }
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

Error _obj::error_manager::error_not_dict(PyObject* o) const {
  return TypeError() << "Expected a dict, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_range(PyObject* o) const {
  return TypeError() << "Expected a range, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_slice(PyObject* o) const {
  return TypeError() << "Expected a slice, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_not_stype(PyObject* o) const {
  return TypeError() << "Expected an stype, instead got " << Py_TYPE(o);
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

Error _obj::error_manager::error_not_iterable(PyObject* o) const {
  return TypeError() << "Expected an iterable, instead got " << Py_TYPE(o);
}

Error _obj::error_manager::error_int_negative(PyObject*) const {
  return ValueError() << "Integer value cannot be negative";
}

}  // namespace py
