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
#ifndef dt_UTILS_EXCEPTIONS_H
#define dt_UTILS_EXCEPTIONS_H
#include <Python.h>
#include <stdint.h>
#include <exception>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include "types.h"



#define MAKE_EXCEPTION_SAFE(fn)                                                \
  static PyObject* pyes_ ## fn(PyObject* self, PyObject* args) {               \
    try {                                                                      \
      return fn(self, args);                                                   \
    } catch (Error& err) {                                                     \
      err.topython();                                                          \
    } catch (std::exception& e) {                                              \
      (Error() << e.what()).topython();                                        \
    }                                                                          \
    return nullptr;                                                            \
  }

class CErrno {};
extern CErrno Errno;


//------------------------------------------------------------------------------

class Error : public std::exception
{
  std::ostringstream error;

public:
  Error(const Error& other);
  Error() = default;

  Error& operator<<(const std::string&);
  Error& operator<<(const char*);
  Error& operator<<(const void*);
  Error& operator<<(int64_t);
  Error& operator<<(int32_t);
  Error& operator<<(int8_t);
  Error& operator<<(size_t);
  Error& operator<<(SType);
  Error& operator<<(const CErrno&);
  Error& operator<<(PyObject*);
  #ifdef __APPLE__
    Error& operator<<(ssize_t);
  #endif

  virtual const char* what() const noexcept;

  /**
   * Translate this exception into a Python error by calling PyErr_SetString
   * with the appropriate exception class and message.
   */
  virtual void topython();

  /**
   * Return Python class corresponding to this exception.
   */
  virtual PyObject* pyclass() const;
};


//------------------------------------------------------------------------------

class PyError : public Error
{
  PyObject* exc_type;
  PyObject* exc_value;
  PyObject* exc_traceback;

public:
  PyError();
  ~PyError();
  void topython() override;
  PyObject* pyclass() const override;
};


//------------------------------------------------------------------------------

class RuntimeError : public Error {
public:
  PyObject* pyclass() const override { return PyExc_RuntimeError; }
};


//------------------------------------------------------------------------------

class TypeError : public Error {
public:
  PyObject* pyclass() const override { return PyExc_TypeError; }
};


//------------------------------------------------------------------------------

class ValueError : public Error {
public:
  PyObject* pyclass() const override { return PyExc_ValueError; }
};


//------------------------------------------------------------------------------

class MemoryError : public Error {
public:
  PyObject* pyclass() const override { return PyExc_MemoryError; }
};


//------------------------------------------------------------------------------

class NotImplError : public Error {
public:
  PyObject* pyclass() const override { return PyExc_NotImplementedError; }
};


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
};


#endif
