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

void init_exceptions();  // called during module initialization



//------------------------------------------------------------------------------

class Error : public std::exception
{
  protected:
    std::ostringstream error_message_;
    // Borrowed reference; however do not use a py::robj here in order
    // to avoid circular dependencies
    PyObject* pycls_;

    // These fields are only used by PyError, and they are owned references
    mutable PyObject* exc_type_;
    mutable PyObject* exc_value_;
    mutable PyObject* exc_traceback_;

  public:
    explicit Error();
    explicit Error(PyObject* cls);
    Error(const Error&);
    Error(Error&&);
    ~Error() override;

    template <typename T>
    Error& operator<<(const T& value) {
      error_message_ << value;
      return *this;
    }

    void to_stderr() const;
    std::string to_string() const;

    bool matches_exception_class(Error(*)()) const;

    /**
     * Translate this exception into a Python error by calling PyErr_SetString
     * with the appropriate exception class and message.
     */
    void to_python() const noexcept;

    // Check whether this is a "KeyboardInterrupt" exception
    bool is_keyboard_interrupt() const noexcept;
};

using PyObjectPtr = PyObject*;
using PyTypeObjectPtr = PyTypeObject*;
template <> Error& Error::operator<<(const dt::CString&);
template <> Error& Error::operator<<(const dt::SType&);
template <> Error& Error::operator<<(const dt::LType&);
template <> Error& Error::operator<<(const py::robj&);
template <> Error& Error::operator<<(const py::oobj&);
template <> Error& Error::operator<<(const py::ostring&);
template <> Error& Error::operator<<(const CErrno&);
template <> Error& Error::operator<<(const PyObjectPtr&);
template <> Error& Error::operator<<(const PyTypeObjectPtr&);
template <> Error& Error::operator<<(const char&);



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
Error PyError();


//------------------------------------------------------------------------------

class Warning : public Error {
  public:
    Warning(PyObject* cls);
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
    PyObject* exc_type_;
    PyObject* exc_value_;
    PyObject* exc_traceback_;

  public:
    HidePythonError();
    ~HidePythonError();
};




#endif
