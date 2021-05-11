//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_PYTHON_DATETIME_h
#define dt_PYTHON_DATETIME_h
#include "python/obj.h"
#include "lib/hh/date.h"
namespace py {


/**
  * Wrapper class around Python's `datetime.datetime` object.
  *
  * The implementation is in "date.cc" (in order to avoid initializing
  * the python datetime capsule more than once).
  */
class odatetime : public oobj {
  public:
    odatetime() = default;
    odatetime(const odatetime&) = default;
    odatetime(odatetime&&) = default;
    odatetime& operator=(const odatetime&) = default;
    odatetime& operator=(odatetime&&) = default;

    explicit odatetime(int64_t time);
    static bool check(py::robj obj);
    static odatetime unchecked(PyObject*);

    int64_t get_time() const;
    static PyTypeObject* type();

  private:
    explicit odatetime(PyObject*);
};



}  // namespace py
#endif
