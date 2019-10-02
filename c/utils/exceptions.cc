//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <algorithm>
#include <iostream>
#include <errno.h>
#include <string.h>
#include "parallel/api.h"
#include "progress/progress_manager.h"
#include "python/obj.h"
#include "python/string.h"
#include "utils/exceptions.h"
#include "utils/assert.h"


// Singleton, used to write the current "errno" into the stream
CErrno Errno;

static PyObject* type_error_class;
static PyObject* value_error_class;
static PyObject* datatable_warning_class;


//==============================================================================

static bool is_string_empty(const char* msg) noexcept {
  if (!msg) return true;
  char c;
  while ((c = *msg)) {
    if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r'))
      return false;
    msg++;
  }
  return true;
}


void exception_to_python(const std::exception& e) noexcept {
  wassert(dt::num_threads_in_team() == 0);
  const Error* error = dynamic_cast<const Error*>(&e);
  if (error) {
    dt::progress::manager->set_error_status(error->is_keyboard_interrupt());
    error->to_python();
  }
  else if (!PyErr_Occurred()) {
    const char* msg = e.what();
    if (is_string_empty(msg)) {
      PyErr_SetString(PyExc_Exception, "unknown error");
    } else {
      PyErr_SetString(PyExc_Exception, msg);
    }
  }
}



//==============================================================================

Error::Error(PyObject* _pycls) : pycls(_pycls) {}

Error::Error(const Error& other) {
  error << other.error.str();
  pycls = other.pycls;
}

Error::Error(Error&& other) : Error() {
  *this = std::move(other);
}

Error& Error::operator=(Error&& other) {
  #if defined(__GNUC__) && __GNUC__ < 5
    // In gcc4.8 string stream was not moveable
    error.str(other.error.str());
  #else
    std::swap(error, other.error);
  #endif
  std::swap(pycls, other.pycls);
  return *this;
}

void Error::to_stderr() const {
  std::cerr << error.str() << "\n";
}

std::string Error::to_string() const {
  return error.str();
}


Error& Error::operator<<(const std::string& v) { error << v; return *this; }
Error& Error::operator<<(const char* v)        { error << v; return *this; }
Error& Error::operator<<(const void* v)        { error << v; return *this; }
Error& Error::operator<<(int64_t v)            { error << v; return *this; }
Error& Error::operator<<(int32_t v)            { error << v; return *this; }
Error& Error::operator<<(int8_t v)             { error << v; return *this; }
Error& Error::operator<<(size_t v)             { error << v; return *this; }
Error& Error::operator<<(uint32_t v)           { error << v; return *this; }
Error& Error::operator<<(float v)              { error << v; return *this; }
Error& Error::operator<<(double v)             { error << v; return *this; }
#ifdef __APPLE__
  Error& Error::operator<<(uint64_t v)         { error << v; return *this; }
  Error& Error::operator<<(ssize_t v)          { error << v; return *this; }
#endif

Error& Error::operator<<(const CString& str) {
  return *this << std::string(str.ch, static_cast<size_t>(str.size));
}

Error& Error::operator<<(const py::_obj& o) {
  return *this << o.to_borrowed_ref();
}

Error& Error::operator<<(const py::ostring& o) {
  PyObject* ptr = o.to_borrowed_ref();
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(ptr, &size);
  if (chardata) {
    error << std::string(chardata, static_cast<size_t>(size));
  } else {
    error << "<unknown>";
    PyErr_Clear();
  }
  return *this;
}

Error& Error::operator<<(PyObject* v) {
  PyObject* repr = PyObject_Repr(v);
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(repr, &size);
  if (chardata) {
    error << std::string(chardata, static_cast<size_t>(size));
  } else {
    error << "<unknown>";
    PyErr_Clear();
  }
  Py_XDECREF(repr);
  return *this;
}

Error& Error::operator<<(PyTypeObject* v) {
  return *this << reinterpret_cast<PyObject*>(v);
}

Error& Error::operator<<(const CErrno&) {
  error << "[errno " << errno << "] " << strerror(errno);
  return *this;
}

Error& Error::operator<<(SType stype) {
  error << info(stype).name();
  return *this;
}

Error& Error::operator<<(LType ltype) {
  error << info::ltype_name(ltype);
  return *this;
}

Error& Error::operator<<(char c) {
  uint8_t uc = static_cast<uint8_t>(c);
  if (uc < 0x20 || uc >= 0x80) {
    uint8_t d1 = uc >> 4;
    uint8_t d2 = uc & 15;
    error << "\\x" << static_cast<char>((d1 <= 9? '0' : 'a' - 10) + d1)
                   << static_cast<char>((d2 <= 9? '0' : 'a' - 10) + d2);
  } else {
    error << c;
  }
  return *this;
}


void Error::to_python() const noexcept {
  // The pointer returned by errstr.c_str() is valid until errstr gets out
  // of scope. By contrast, `error.str().c_str()` returns a dangling pointer,
  // which usually works but sometimes doesn't...
  // See https://stackoverflow.com/questions/1374468
  try {
    const std::string errstr = error.str();
    PyErr_SetString(pycls, errstr.c_str());
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
  }
}


bool Error::is_keyboard_interrupt() const noexcept {
  return false;
}



//==============================================================================

PyError::PyError() {
  PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);
}

PyError::PyError(PyError&& other) : Error(std::move(other)) {
  exc_type = other.exc_type;
  exc_value = other.exc_value;
  exc_traceback = other.exc_traceback;
  other.exc_type = nullptr;
  other.exc_value = nullptr;
  other.exc_traceback = nullptr;
}

PyError::~PyError() {
  Py_XDECREF(exc_type);
  Py_XDECREF(exc_value);
  Py_XDECREF(exc_traceback);
}

void PyError::to_python() const noexcept {
  PyErr_Restore(exc_type, exc_value, exc_traceback);
  exc_type = nullptr;
  exc_value = nullptr;
  exc_traceback = nullptr;
}

bool PyError::is_keyboard_interrupt() const noexcept {
  return exc_type == PyExc_KeyboardInterrupt;
}

bool PyError::is_assertion_error() const noexcept {
  return exc_type == PyExc_AssertionError;
}

std::string PyError::message() const {
  return py::robj(exc_value).to_pystring_force().to_string();
}



//==============================================================================

Error AssertionError() { return Error(PyExc_AssertionError); }
Error ImportError()    { return Error(PyExc_ImportError); }
Error IOError()        { return Error(PyExc_IOError); }
Error MemoryError()    { return Error(PyExc_MemoryError); }
Error NotImplError()   { return Error(PyExc_NotImplementedError); }
Error OverflowError()  { return Error(PyExc_OverflowError); }
Error RuntimeError()   { return Error(PyExc_RuntimeError); }
Error TypeError()      { return Error(type_error_class); }
Error ValueError()     { return Error(value_error_class); }

void replace_typeError(PyObject* obj) { type_error_class = obj; }
void replace_valueError(PyObject* obj) { value_error_class = obj; }
void replace_dtWarning(PyObject* obj) { datatable_warning_class = obj; }

void init_exceptions() {
  type_error_class = PyExc_TypeError;
  value_error_class = PyExc_ValueError;
  datatable_warning_class = PyExc_Warning;
}



//==============================================================================

Warning::Warning(PyObject* cls) : Error(cls) {}

void Warning::emit() {
  const std::string errstr = error.str();
  // Normally, PyErr_WarnEx returns 0. However, when the `warnings` module is
  // configured in such a way that all warnings are converted into errors,
  // then PyErr_WarnEx will return -1. At that point we should throw
  // an exception too, the error message is already set in Python.
  int ret = PyErr_WarnEx(pycls, errstr.c_str(), 1);
  if (ret) throw PyError();
}


Warning DatatableWarning()  { return Warning(datatable_warning_class); }
// Note, DeprecationWarnings are ignored by default in python
Warning DeprecationWarning() { return Warning(PyExc_FutureWarning); }

