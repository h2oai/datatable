//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#ifndef dt_CALL_LOGGER_h
#define dt_CALL_LOGGER_h
#include "_dt.h"
#include "python/python.h"
namespace dt {


/**
  * This class is a guardian object that implements logging of info
  * about various function calls. Notably, we want to be able to log
  * all calls from Python into C; but in theory we could also log
  * some internal calls in the same manner.
  */
class CallLogger {
  class Impl;
  class Lock;
  private:
    static std::vector<Impl*> impl_cache_;
    static size_t nested_level_;

    Impl* impl_;

  public:
    static PyObject* GETITEM;
    static PyObject* DELITEM;
    enum Op : int {
      __add__      = 0,
      __sub__      = 1,
      __mul__      = 2,
      __mod__      = 3,
      __divmod__   = 4,
      __pow__      = 5,
      __lshift__   = 6,
      __rshift__   = 7,
      __and__      = 8,
      __or__       = 9,
      __xor__      = 10,
      __truediv__  = 11,
      __floordiv__ = 12,
      __neg__      = 13,
      __pos__      = 14,
      __abs__      = 15,
      __invert__   = 16,
      __bool__     = 17,
      __int__      = 18,
      __float__    = 19,
      __repr__     = 20,
      __str__      = 21,
      __iter__     = 22,
      __next__     = 23,
    };

    CallLogger(CallLogger&&) noexcept;
    CallLogger(const CallLogger&) = delete;
    ~CallLogger();

    static CallLogger function  (const py::PKArgs*, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger function  (const py::XArgs*, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger method    (const py::PKArgs*, PyObject* pyobj, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger method    (const py::XArgs*, PyObject* pyobj, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger dealloc   (PyObject* pyobj) noexcept;
    static CallLogger getset    (PyObject* pyobj, PyObject* val, void* closure) noexcept;
    static CallLogger getattr   (PyObject* pyobj, PyObject* key) noexcept;
    static CallLogger getsetitem(PyObject* pyobj, PyObject* key, PyObject* val) noexcept;
    static CallLogger getbuffer (PyObject* pyobj, Py_buffer* buf, int flags) noexcept;
    static CallLogger delbuffer (PyObject* pyobj, Py_buffer* buf) noexcept;
    static CallLogger len       (PyObject* pyobj) noexcept;
    static CallLogger hash      (PyObject* pyobj) noexcept;
    static CallLogger unaryfn   (PyObject* pyobj, int op) noexcept;
    static CallLogger binaryfn  (PyObject* pyobj, PyObject* other, int op) noexcept;
    static CallLogger ternaryfn (PyObject* x, PyObject* y, PyObject* z, int op) noexcept;
    static CallLogger cmpfn     (PyObject* x, PyObject* y, int op) noexcept;

    // Called once during module initialization
    static void init_options();

  private:
    CallLogger() noexcept;  // used by static constructors
};



// Stop issuing CallLogger messages for the duration of the lock
class CallLoggerLock {
  private:
    bool enabled_previously_;

  public:
    CallLoggerLock();
    ~CallLoggerLock();
};



}  // namespace dt
#endif
