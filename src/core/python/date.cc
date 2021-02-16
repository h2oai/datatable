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
#include <Python.h>
#include <datetime.h>     // Python "datetime" module, not included in Python.h
#include "python/date.h"
#include "lib/hh/date.h"
namespace py {


odate::odate(PyObject* obj) : oobj(obj) {}  // private

odate odate::unchecked(PyObject* obj) {
  return odate(obj);
}


odate::odate(hh::ymd date) {
  v = PyDate_FromDate(date.year, date.month, date.day);
  if (!v) throw PyError();
}

odate::odate(int32_t days)
  : odate(hh::civil_from_days(days)) {}



bool odate::check(robj obj) {
  return PyDate_Check(obj.to_borrowed_ref());
}


int odate::get_days() const {
  return hh::days_from_civil(
      PyDateTime_GET_YEAR(v),
      PyDateTime_GET_MONTH(v),
      PyDateTime_GET_DAY(v)
  );
}


void odate::init() {
  // In order to be able to use python API to access datetime objects,
  // we need to "import" it via a special macro. This macro loads the
  // datetime module as a capsule and stores it in PyDateTimeAPI variable.
  //
  // Note that the PyDateTimeAPI variable will be local to this file,
  // which means that no PyDateTime* macros will work outside of this
  // translation unit.
  //
  // See https://docs.python.org/3/c-api/datetime.html
  PyDateTime_IMPORT;
}


}  // namespace py
