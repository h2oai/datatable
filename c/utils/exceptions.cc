//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "utils/exceptions.h"
#include <algorithm>
#include <errno.h>
#include <string.h>


// Singleton, used to write the current "errno" into the stream
CErrno Errno;

static PyObject* type_error_class;
static PyObject* value_error_class;



//==============================================================================

static bool is_string_empty(const char* msg) {
  if (!msg) return true;
  char c;
  while ((c = *msg)) {
    if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r'))
      return false;
    msg++;
  }
  return true;
}


void exception_to_python(const std::exception& e) {
  const Error* error = dynamic_cast<const Error*>(&e);
  if (error) {
    error->topython();
  } else if (!PyErr_Occurred()) {
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
  swap(*this, other);
}

void swap(Error& first, Error& second) noexcept {
  using std::swap;
  swap(first.error, second.error);
  swap(first.pycls, second.pycls);
}


Error& Error::operator<<(const std::string& v) { error << v; return *this; }
Error& Error::operator<<(const char* v)        { error << v; return *this; }
Error& Error::operator<<(const void* v)        { error << v; return *this; }
Error& Error::operator<<(int64_t v)            { error << v; return *this; }
Error& Error::operator<<(int32_t v)            { error << v; return *this; }
Error& Error::operator<<(int8_t v)             { error << v; return *this; }
Error& Error::operator<<(size_t v)             { error << v; return *this; }
#ifdef __APPLE__
  Error& Error::operator<<(ssize_t v)          { error << v; return *this; }
#endif

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

Error& Error::operator<<(const CErrno&) {
  error << "[errno " << errno << "] " << strerror(errno);
  return *this;
}

Error& Error::operator<<(SType stype) {
  error << stype_info[stype].code2;
  return *this;
}


void Error::topython() const {
  // The pointer returned by errstr.c_str() is valid until errstr gets out
  // of scope. By contrast, `error.str().c_str()` returns a dangling pointer,
  // which usually works but sometimes doesn't...
  // See https://stackoverflow.com/questions/1374468
  const std::string& errstr = error.str();
  PyErr_SetString(pycls, errstr.c_str());
}



//==============================================================================

PyError::PyError() {
  PyErr_Fetch(&exc_type, &exc_value, &exc_traceback);
}

PyError::~PyError() {
  Py_XDECREF(exc_type);
  Py_XDECREF(exc_value);
  Py_XDECREF(exc_traceback);
}

void PyError::topython() const {
  PyErr_Restore(exc_type, exc_value, exc_traceback);
  exc_type = nullptr;
  exc_value = nullptr;
  exc_traceback = nullptr;
}

bool PyError::is_keyboard_interrupt() const {
  return exc_type == PyExc_KeyboardInterrupt;
}



//==============================================================================

Error RuntimeError()  { return Error(PyExc_RuntimeError); }
Error TypeError()     { return Error(type_error_class); }
Error ValueError()    { return Error(value_error_class); }
Error OverflowError() { return Error(PyExc_OverflowError); }
Error MemoryError()   { return Error(PyExc_MemoryError); }
Error NotImplError()  { return Error(PyExc_NotImplementedError); }
Error IOError()       { return Error(PyExc_IOError); }
Error AssertionError(){ return Error(PyExc_AssertionError); }

void replace_typeError(PyObject* obj) { type_error_class = obj; }
void replace_valueError(PyObject* obj) { value_error_class = obj; }

void init_exceptions() {
  type_error_class = PyExc_TypeError;
  value_error_class = PyExc_ValueError;
}



//==============================================================================

OmpExceptionManager::OmpExceptionManager() : ptr(nullptr) {}

bool OmpExceptionManager::exception_caught() {
  return bool(ptr);
}

void OmpExceptionManager::capture_exception() {
  std::unique_lock<std::mutex> guard(this->lock);
  if (!ptr) ptr = std::current_exception();
}

void OmpExceptionManager::rethrow_exception_if_any() {
  if (ptr) std::rethrow_exception(ptr);
}
