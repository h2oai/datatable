//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "exceptions.h"
#include <errno.h>
#include <string.h>


// Singleton, used to write the current "errno" into the stream
CErrno Errno;


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
Error& Error::operator<<(ssize_t v)            { error << v; return *this; }

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


const char* Error::what() const noexcept {
  return error.str().c_str();
}

void Error::topython() {
  PyErr_SetString(pyclass(), what());
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

void PyError::topython() {
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
