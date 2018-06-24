//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_EXCEPTIONS_h
#define dt_UTILS_EXCEPTIONS_h
#include <Python.h>
#include <stdint.h>
#include <exception>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include "types.h"

class CErrno {};
extern CErrno Errno;

void exception_to_python(const std::exception&);


//------------------------------------------------------------------------------

class Error : public std::exception
{
  std::ostringstream error;
  PyObject* pycls;  // This pointer acts as an enum...

public:
  Error(PyObject* cls = PyExc_Exception);
  Error(const Error& other);
  Error(Error&& other);
  virtual ~Error() override {}
  friend void swap(Error& first, Error& second) noexcept;

  Error& operator<<(const std::string&);
  Error& operator<<(const char*);
  Error& operator<<(const void*);
  Error& operator<<(int64_t);
  Error& operator<<(int32_t);
  Error& operator<<(int8_t);
  Error& operator<<(char);
  Error& operator<<(size_t);
  Error& operator<<(uint32_t);
  Error& operator<<(SType);
  Error& operator<<(const CErrno&);
  Error& operator<<(PyObject*);
  #ifdef __APPLE__
    Error& operator<<(uint64_t);
    Error& operator<<(ssize_t);
  #endif

  /**
   * Translate this exception into a Python error by calling PyErr_SetString
   * with the appropriate exception class and message.
   */
  virtual void topython() const;
};


//------------------------------------------------------------------------------

class PyError : public Error
{
  mutable PyObject* exc_type;
  mutable PyObject* exc_value;
  mutable PyObject* exc_traceback;

public:
  PyError();
  ~PyError() override;
  void topython() const override;
  bool is_keyboard_interrupt() const;
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

void replace_typeError(PyObject* obj);
void replace_valueError(PyObject* obj);
void init_exceptions();


//------------------------------------------------------------------------------

/**
 * Helper class for dealing with exceptions inside OMP code: it allows one to
 * capture exceptions that occur, and then re-throw them later after the OMP
 * region.
 * ----
 * Adapted from StackOverflow question <stackoverflow.com/questions/11828539>
 * by user Grizzly <stackoverflow.com/users/201270/grizzly>.
 * Licensed under CC BY-SA 3.0 <http://creativecommons.org/licenses/by-sa/3.0/>
 */
class OmpExceptionManager
{
  std::exception_ptr ptr;
  std::mutex lock;

public:
  OmpExceptionManager();
  bool exception_caught();
  void capture_exception();
  void rethrow_exception_if_any();

  bool is_keyboard_interrupt();
};


#endif
