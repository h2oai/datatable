//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <algorithm>
#include <iostream>
#include <errno.h>
#include <string.h>
#include "cstring.h"
#include "ltype.h"
#include "parallel/api.h"
#include "progress/progress_manager.h"
#include "python/obj.h"
#include "python/python.h"
#include "python/string.h"
#include "python/tuple.h"
#include "stype.h"
#include "utils/assert.h"
#include "utils/exceptions.h"


// Singleton, used to write the current "errno" into the stream
CErrno Errno;

static PyObject* DtExc_ImportError           = PyExc_Exception;
static PyObject* DtExc_IndexError            = PyExc_Exception;
static PyObject* DtExc_InvalidOperationError = PyExc_Exception;
static PyObject* DtExc_IOError               = PyExc_Exception;
static PyObject* DtExc_KeyError              = PyExc_Exception;
static PyObject* DtExc_MemoryError           = PyExc_Exception;
static PyObject* DtExc_NotImplementedError   = PyExc_Exception;
static PyObject* DtExc_OverflowError         = PyExc_Exception;
static PyObject* DtExc_TypeError             = PyExc_Exception;
static PyObject* DtExc_ValueError            = PyExc_Exception;
static PyObject* DtWrn_DatatableWarning      = PyExc_Exception;
static PyObject* DtWrn_IOWarning             = PyExc_Exception;

void init_exceptions() {
  auto dx = py::oobj::import("datatable", "exceptions");
  DtExc_ImportError           = dx.get_attr("ImportError").release();
  DtExc_IndexError            = dx.get_attr("IndexError").release();
  DtExc_InvalidOperationError = dx.get_attr("InvalidOperationError").release();
  DtExc_IOError               = dx.get_attr("IOError").release();
  DtExc_KeyError              = dx.get_attr("KeyError").release();
  DtExc_MemoryError           = dx.get_attr("MemoryError").release();
  DtExc_NotImplementedError   = dx.get_attr("NotImplementedError").release();
  DtExc_OverflowError         = dx.get_attr("OverflowError").release();
  DtExc_TypeError             = dx.get_attr("TypeError").release();
  DtExc_ValueError            = dx.get_attr("ValueError").release();
  DtWrn_DatatableWarning      = dx.get_attr("DatatableWarning").release();
  DtWrn_IOWarning             = dx.get_attr("IOWarning").release();
}



//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static bool is_string_empty(const char* msg) noexcept {
  if (!msg) return true;
  char c;
  while ((c = *msg) != 0) {
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


/**
  * If `str` contains any backticks or backslashes, they will be
  * escaped by prepending each such character with a backslash.
  * If there are no backticks/backslahes in `str`, then a simple copy
  * of the string is returned.
  */
std::string escape_backticks(const std::string& str) {
  size_t count = 0;
  for (char c : str) {
    count += (c == '`' || c == '\\');
  }
  if (count == 0) return str;
  std::string out;
  out.reserve(str.size() + count);
  for (char c : str) {
    if (c == '`' || c == '\\') out += '\\';
    out += c;
  }
  return out;
}




//------------------------------------------------------------------------------
// Error: constructors
//------------------------------------------------------------------------------

Error::Error()
  : pycls_(nullptr),
    exc_type_(nullptr),
    exc_value_(nullptr),
    exc_traceback_(nullptr)
{
  PyErr_Fetch(&exc_type_, &exc_value_, &exc_traceback_);
  if (is_keyboard_interrupt()) {
    dt::progress::manager->set_status_cancelled();
  }
}


Error::Error(PyObject* exception_class)
  : pycls_(exception_class),
    exc_type_(nullptr),
    exc_value_(nullptr),
    exc_traceback_(nullptr) {}


Error::Error(const Error& other) {
  error_message_ << other.error_message_.str();
  pycls_ = other.pycls_;
  exc_type_      = other.exc_type_;       Py_XINCREF(exc_type_);
  exc_value_     = other.exc_value_;      Py_XINCREF(exc_value_);
  exc_traceback_ = other.exc_traceback_;  Py_XINCREF(exc_traceback_);
}

Error::Error(Error&& other) : Error(nullptr) {
  std::swap(error_message_, other.error_message_);
  std::swap(pycls_, other.pycls_);
  std::swap(exc_type_, other.exc_type_);
  std::swap(exc_value_, other.exc_value_);
  std::swap(exc_traceback_, other.exc_traceback_);
}

Error::~Error() {
  Py_XDECREF(exc_type_);      exc_type_ = nullptr;
  Py_XDECREF(exc_value_);     exc_value_ = nullptr;
  Py_XDECREF(exc_traceback_); exc_traceback_ = nullptr;
}




//------------------------------------------------------------------------------
// Error: message logging methods
//------------------------------------------------------------------------------

template <>
Error& Error::operator<<(const dt::CString& str) {
  return *this << str.to_string();
}


template <>
Error& Error::operator<<(const py::robj& o) {
  return *this << o.to_borrowed_ref();
}


template <>
Error& Error::operator<<(const py::oobj& o) {
  return *this << o.to_borrowed_ref();
}


template <>
Error& Error::operator<<(const py::ostring& o) {
  PyObject* ptr = o.to_borrowed_ref();
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(ptr, &size);
  if (chardata) {
    error_message_ << std::string(chardata, static_cast<size_t>(size));
  } else {
    error_message_ << "<unknown>";
    PyErr_Clear();
  }
  return *this;
}


template <>
Error& Error::operator<<(const PyObjectPtr& v) {
  PyObject* repr = PyObject_Repr(v);
  Py_ssize_t size;
  const char* chardata = PyUnicode_AsUTF8AndSize(repr, &size);
  if (chardata) {
    error_message_ << std::string(chardata, static_cast<size_t>(size));
  } else {
    error_message_ << "<unknown>";
    PyErr_Clear();
  }
  Py_XDECREF(repr);
  return *this;
}


template <>
Error& Error::operator<<(const PyTypeObjectPtr& v) {
  return *this << reinterpret_cast<const PyObjectPtr>(v);
}


template <>
Error& Error::operator<<(const CErrno&) {
  error_message_ << "[errno " << errno << "] " << strerror(errno);
  return *this;
}


template <>
Error& Error::operator<<(const dt::SType& stype) {
  error_message_ << dt::stype_name(stype);
  return *this;
}


template <>
Error& Error::operator<<(const dt::LType& ltype) {
  error_message_ << dt::ltype_name(ltype);
  return *this;
}


template <>
Error& Error::operator<<(const dt::Type& type) {
  error_message_ << type.to_string();
  return *this;
}


template <>
Error& Error::operator<<(const char& c) {
  uint8_t uc = static_cast<uint8_t>(c);
  if (uc < 0x20 || uc >= 0x80 || uc == '`' || uc == '\\') {
    error_message_ << '\\';
    if (c == '\n') error_message_ << 'n';
    else if (c == '\r') error_message_ << 'r';
    else if (c == '\t') error_message_ << 't';
    else if (c == '\\') error_message_ << '\\';
    else if (c == '`')  error_message_ << '`';
    else {
      uint8_t d1 = uc >> 4;
      uint8_t d2 = uc & 15;
      error_message_ << "\\x" << static_cast<char>((d1 <= 9? '0' : 'a' - 10) + d1)
                     << static_cast<char>((d2 <= 9? '0' : 'a' - 10) + d2);
    }
  } else {
    error_message_ << c;
  }
  return *this;
}




//------------------------------------------------------------------------------
// Error: misc methods
//------------------------------------------------------------------------------

void Error::to_stderr() const {
  std::cerr << to_string() << "\n";
}

std::string Error::to_string() const {
  if (pycls_) {
    return error_message_.str();
  }
  else {
    py::ostring msg = py::robj(exc_value_).to_pystring_force();
    return msg.to_string();
  }
}


// This will not work with PyError though
bool Error::matches_exception_class(Error(*exc)()) const {
  return exc().pycls_ == pycls_;
}


void Error::to_python() const noexcept {
  // The pointer returned by errstr.c_str() is valid until errstr gets out
  // of scope. By contrast, `error_message_.str().c_str()` returns a dangling pointer,
  // which usually works but sometimes doesn't...
  // See https://stackoverflow.com/questions/1374468
  try {
    if (pycls_) {
      auto errstr = to_string();
      PyErr_SetString(pycls_, errstr.c_str());
    } else {
      // This call takes away a reference to each object: you must own a
      // reference to each object before the call and after the call you no
      // longer own these references.
      PyErr_Restore(exc_type_, exc_value_, exc_traceback_);
      exc_type_ = nullptr;
      exc_value_ = nullptr;
      exc_traceback_ = nullptr;
    }
  }
  catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError, e.what());
  }
}


bool Error::is_keyboard_interrupt() const noexcept {
  return exc_type_ == PyExc_KeyboardInterrupt;
}


void Error::emit_warning() const {
  auto errstr = to_string();
  // Normally, PyErr_WarnEx returns 0. However, when the `warnings` module is
  // configured in such a way that all warnings are converted into errors,
  // then PyErr_WarnEx will return -1. At that point we should throw
  // an exception too, the error message is already set in Python.
  int ret = PyErr_WarnEx(pycls_, errstr.c_str(), 1);
  if (ret) throw PyError();
}




//==============================================================================

Error AssertionError()        { return Error(PyExc_AssertionError); }
Error AttributeError()        { return Error(PyExc_AttributeError); }
Error RuntimeError()          { return Error(PyExc_RuntimeError); }
Error ImportError()           { return Error(DtExc_ImportError); }
Error IndexError()            { return Error(DtExc_IndexError); }
Error IOError()               { return Error(DtExc_IOError); }
Error KeyError()              { return Error(DtExc_KeyError); }
Error MemoryError()           { return Error(DtExc_MemoryError); }
Error NotImplError()          { return Error(DtExc_NotImplementedError); }
Error OverflowError()         { return Error(DtExc_OverflowError); }
Error TypeError()             { return Error(DtExc_TypeError); }
Error ValueError()            { return Error(DtExc_ValueError); }
Error InvalidOperationError() { return Error(DtExc_InvalidOperationError); }
Error PyError()               { return Error(); }

Error DeprecationWarning() { return Error(PyExc_FutureWarning); }
Error DatatableWarning()   { return Error(DtWrn_DatatableWarning); }
Error IOWarning()          { return Error(DtWrn_IOWarning); }




//------------------------------------------------------------------------------
// HidePythonError
//------------------------------------------------------------------------------

HidePythonError::HidePythonError() {
  if (PyErr_Occurred()) {
    PyErr_Fetch(&exc_type_, &exc_value_, &exc_traceback_);
  } else {
    exc_type_ = nullptr;
    exc_value_ = nullptr;
    exc_traceback_ = nullptr;
  }
}

HidePythonError::~HidePythonError() {
  if (exc_type_) {
    PyErr_Restore(exc_type_, exc_value_, exc_traceback_);
  }
}
