//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_EXCEPTIONS_h
#define dt_UTILS_EXCEPTIONS_h
#include <cstdint>
#include <exception>  // std::exception
#include <sstream>    // std::ostringstream
#include <string>     // std::string
#include "types.h"

class CErrno {};
extern CErrno Errno;

void exception_to_python(const std::exception&) noexcept;

namespace py {
  class _obj;
  class ostring;
}


//------------------------------------------------------------------------------

class Error : public std::exception
{
  protected:
    std::ostringstream error;
    PyObject* pycls;  // This pointer acts as an enum...

public:
  Error(PyObject* cls = PyExc_Exception);
  Error(const Error& other);
  Error(Error&& other);
  Error& operator=(Error&& other);
  virtual ~Error() override {}

  Error& operator<<(const std::string&);
  Error& operator<<(const char*);
  Error& operator<<(const void*);
  Error& operator<<(const CString&);
  Error& operator<<(int64_t);
  Error& operator<<(int32_t);
  Error& operator<<(int8_t);
  Error& operator<<(char);
  Error& operator<<(size_t);
  Error& operator<<(uint32_t);
  Error& operator<<(float);
  Error& operator<<(double);
  Error& operator<<(SType);
  Error& operator<<(LType);
  Error& operator<<(const CErrno&);
  Error& operator<<(const py::_obj&);
  Error& operator<<(const py::ostring&);
  Error& operator<<(PyObject*);
  Error& operator<<(PyTypeObject*);
  #ifdef __APPLE__
    Error& operator<<(uint64_t);
    Error& operator<<(ssize_t);
  #endif

  void to_stderr() const;
  std::string to_string() const;

  /**
   * Translate this exception into a Python error by calling PyErr_SetString
   * with the appropriate exception class and message.
   */
  virtual void to_python() const noexcept;

  // Check whether this is a "KeyboardInterrupt" exception
  virtual bool is_keyboard_interrupt() const noexcept;
};


//------------------------------------------------------------------------------

class PyError : public Error
{
  mutable PyObject* exc_type;
  mutable PyObject* exc_value;
  mutable PyObject* exc_traceback;

public:
  PyError();
  PyError(PyError&&);
  virtual ~PyError() override;

  void to_python() const noexcept override;
  bool is_keyboard_interrupt() const noexcept override;
  bool is_assertion_error() const noexcept;
  std::string message() const;
};


//------------------------------------------------------------------------------

Error RuntimeError();
Error TypeError();
Error ValueError();
Error OverflowError();
Error MemoryError();
Error NotImplError();
Error IOError();
Error AssertionError();
Error ImportError();

void replace_typeError(PyObject* obj);
void replace_valueError(PyObject* obj);
void replace_dtWarning(PyObject* obj);
void init_exceptions();


//------------------------------------------------------------------------------

class Warning : public Error {
  public:
    Warning(PyObject* cls);
    Warning(const Warning&) = default;

    void emit();
};

Warning DatatableWarning();
Warning DeprecationWarning();



#endif
