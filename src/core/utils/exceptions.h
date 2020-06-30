//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#ifndef dt_UTILS_EXCEPTIONS_h
#define dt_UTILS_EXCEPTIONS_h
#include <exception>  // std::exception
#include <sstream>    // std::ostringstream
#include "_dt.h"

class CErrno {};
extern CErrno Errno;


void exception_to_python(const std::exception&) noexcept;

std::string escape_backticks(const std::string&);


//------------------------------------------------------------------------------

class Error : public std::exception
{
  protected:
    std::ostringstream error;
    // Owned reference; however do not use a py::oobj here in order
    // to avoid circular dependencies
    PyObject* pycls;

public:
  Error(PyObject* cls);
  Error(py::oobj cls);
  Error(const Error& other);
  Error(Error&& other);
  Error& operator=(Error&& other);
  virtual ~Error() override {}

  Error& operator<<(const std::string&);
  Error& operator<<(const char*);
  Error& operator<<(const void*);
  Error& operator<<(const dt::CString&);
  Error& operator<<(int64_t);
  Error& operator<<(int32_t);
  Error& operator<<(int8_t);
  Error& operator<<(char);
  Error& operator<<(size_t);
  Error& operator<<(uint32_t);
  Error& operator<<(float);
  Error& operator<<(double);
  Error& operator<<(dt::SType);
  Error& operator<<(dt::LType);
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

Error AssertionError();
Error ImportError();
Error IndexError();
Error InvalidOperationError();
Error IOError();
Error KeyError();
Error MemoryError();
Error NotImplError();
Error OverflowError();
Error RuntimeError();
Error TypeError();
Error ValueError();



//------------------------------------------------------------------------------

class Warning : public Error {
  public:
    Warning(PyObject* cls);
    Warning(py::oobj cls);
    Warning(const Warning&) = default;

    void emit();
};

Warning DatatableWarning();
Warning DeprecationWarning();
Warning IOWarning();




//------------------------------------------------------------------------------
// HidePythonError
//------------------------------------------------------------------------------

/**
  * Guardian-type class that will temporarily hide the current
  * python error (if any), restoring it when the object goes out of
  * scope.
  */
class HidePythonError {
  private:
    PyObject* ptype_;
    PyObject* pvalue_;
    PyObject* ptraceback_;

  public:
    HidePythonError();
    ~HidePythonError();
};




#endif
