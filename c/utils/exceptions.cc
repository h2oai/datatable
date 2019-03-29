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
#include "python/obj.h"
#include "python/string.h"


// Singleton, used to write the current "errno" into the stream
CErrno Errno;

static PyObject* type_error_class;
static PyObject* value_error_class;
static PyObject* datatable_warning_class;


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
  #if defined(__GNUC__) && __GNUC__ < 5
    // In gcc4.8 string stream was not moveable
    error << other.error.str();
  #else
    std::swap(error, other.error);
  #endif
  std::swap(pycls, other.pycls);
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


void Error::topython() const {
  // The pointer returned by errstr.c_str() is valid until errstr gets out
  // of scope. By contrast, `error.str().c_str()` returns a dangling pointer,
  // which usually works but sometimes doesn't...
  // See https://stackoverflow.com/questions/1374468
  const std::string errstr = error.str();
  PyErr_SetString(pycls, errstr.c_str());
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

void PyError::topython() const {
  PyErr_Restore(exc_type, exc_value, exc_traceback);
  exc_type = nullptr;
  exc_value = nullptr;
  exc_traceback = nullptr;
}

bool PyError::is_keyboard_interrupt() const {
  return exc_type == PyExc_KeyboardInterrupt;
}

bool PyError::is_assertion_error() const {
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

Warning::~Warning() {
  const std::string errstr = error.str();
  PyErr_WarnEx(pycls, errstr.c_str(), 1);
}


Warning DatatableWarning()  { return Warning(datatable_warning_class); }
// Note, DeprecationWarnings are ignored by default in python
Warning DeprecationWarning() { return Warning(PyExc_FutureWarning); }



//==============================================================================

OmpExceptionManager::OmpExceptionManager() : ptr(nullptr), stop(false) {}


bool OmpExceptionManager::stop_requested() const {
  return stop;
}

bool OmpExceptionManager::exception_caught() const {
  return bool(ptr);
}

void OmpExceptionManager::capture_exception() {
  std::unique_lock<std::mutex> guard(this->lock);
  if (!ptr) ptr = std::current_exception();
  stop = true;
}

void OmpExceptionManager::stop_iterations() {
  std::unique_lock<std::mutex> guard(this->lock);
  stop = true;
}

void OmpExceptionManager::rethrow_exception_if_any() const {
  if (ptr) std::rethrow_exception(ptr);
}

bool OmpExceptionManager::is_keyboard_interrupt() const {
  if (!ptr) return false;
  bool ret = false;
  try {
    std::rethrow_exception(ptr);
  } catch (PyError& e) {
    ret = e.is_keyboard_interrupt();
  } catch (...) {}
  return ret;
}
