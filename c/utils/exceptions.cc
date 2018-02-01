//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "utils/exceptions.h"
#include <errno.h>
#include <string.h>


// Singleton, used to write the current "errno" into the stream
CErrno Errno;


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

Error::Error(const Error& other) {
  error << other.error.str();
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
  char* chardata = PyUnicode_AsUTF8AndSize(repr, &size);
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
  PyErr_SetString(pyclass(), errstr.c_str());
}

PyObject* Error::pyclass() const {
  return PyExc_Exception;
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

PyObject* PyError::pyclass() const {
  return exc_type;
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
