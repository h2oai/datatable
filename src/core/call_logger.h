//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
namespace dt {


/**
  * This class is a guardian object that implements logging of info
  * about various function calls. Notably, we want to be able to log
  * all calls from Python into C; but in theory we could also log
  * some internal calls in the same manner.
  */
class CallLogger {
  class Impl;
  private:
    static std::vector<Impl*> impl_cache_;
    static size_t nested_level_;

    Impl* impl_;

  public:
    CallLogger(CallLogger&&) noexcept;
    CallLogger(const CallLogger&) = delete;
    ~CallLogger();

    static CallLogger function  (const py::PKArgs*, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger method    (const py::PKArgs*, PyObject* pythis, PyObject* pyargs, PyObject* pykwds) noexcept;
    static CallLogger dealloc   (PyObject* pythis) noexcept;
    static CallLogger getsetattr(PyObject* pythis, PyObject* val, void* closure) noexcept;
    static CallLogger getsetitem(PyObject* pythis, PyObject* key, PyObject* val) noexcept;

    // Called once during module initialization
    static void init_options();

  private:
    CallLogger() noexcept;  // used by static constructors
};



}  // namespace dt
#endif
