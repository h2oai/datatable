//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
// See https://docs.python.org/3/c-api/slice.html
// for the details of Python API
//------------------------------------------------------------------------------
#include "python/int.h"
#include "python/slice.h"
#include "utils/assert.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

oslice::oslice(int64_t start, int64_t stop, int64_t step) {
  PyObject* ostart = (start == oslice::NA)? nullptr : PyLong_FromLong(start);
  PyObject* ostop  = (stop == oslice::NA)? nullptr : PyLong_FromLong(stop);
  PyObject* ostep  = (step == oslice::NA)? nullptr : PyLong_FromLong(step);
  v = PySlice_New(ostart, ostop, ostep);
  Py_XDECREF(ostart);
  Py_XDECREF(ostop);
  Py_XDECREF(ostep);
}


// private constructor
oslice::oslice(const robj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Numeric slice
//------------------------------------------------------------------------------

bool oslice::is_numeric() const {
  auto w = reinterpret_cast<PySliceObject*>(v);
  return (w->start == Py_None || PyLong_Check(w->start)) &&
         (w->stop == Py_None  || PyLong_Check(w->stop)) &&
         (w->step == Py_None  || PyLong_Check(w->step));
}

int64_t oslice::start() const {
  oint start = robj(reinterpret_cast<PySliceObject*>(v)->start).to_pyint();
  if (!start) return oslice::NA;
  int overflow;
  return start.ovalue<int64_t>(&overflow);
}

int64_t oslice::stop() const {
  oint stop = robj(reinterpret_cast<PySliceObject*>(v)->stop).to_pyint();
  if (!stop) return oslice::NA;
  int overflow;
  return stop.ovalue<int64_t>(&overflow);
}

int64_t oslice::step() const {
  oint step = robj(reinterpret_cast<PySliceObject*>(v)->step).to_pyint();
  if (!step) return oslice::NA;
  int overflow;
  return step.ovalue<int64_t>(&overflow);
}


void oslice::normalize(
    size_t len, size_t* start, size_t* count, size_t* step) const
{
  int64_t ilen = static_cast<int64_t>(len);
  int64_t istep = this->step();
  if (istep == oslice::NA) istep = 1;
  if (istep == 0) {
    throw ValueError() << "Slice step cannot be zero";
  }

  int64_t istart = this->start();
  if (istart == oslice::NA) istart = (istep > 0)? 0 : ilen - 1;
  if (istart < 0) istart += ilen;
  if (istart < 0) istart = 0;
  if (istart > ilen) istart = ilen;
  xassert(istart >= 0 && istart <= ilen);

  int64_t istop = this->stop();
  if (istop == oslice::NA) {
    istop = (istep > 0)? ilen : -1;
  }
  else {
    if (istop < 0) istop += ilen;
    if (istop < 0) istop = -1;
    if (istop > ilen) istop = ilen;
  }
  xassert(istop >= -1 && istop <= ilen);

  int64_t icount = 0;
  if (istep > 0 && istop > istart) {
    icount = (istop - istart) / istep;
  }
  if (istep < 0 && istop < istart) {
    icount = (istart - istop) / (-istep);
  }
  xassert(icount >= 0 && icount < ilen);

  *start = static_cast<size_t>(istart);
  *count = static_cast<size_t>(icount);
  *step  = static_cast<size_t>(istep);
}



//------------------------------------------------------------------------------
// String slice
//------------------------------------------------------------------------------

bool oslice::is_string() const {
  auto w = reinterpret_cast<PySliceObject*>(v);
  return (w->start == Py_None || PyUnicode_Check(w->start)) &&
         (w->stop == Py_None  || PyUnicode_Check(w->stop)) &&
         (w->step == Py_None);
}

oobj oslice::start_obj() const {
  return oobj(reinterpret_cast<PySliceObject*>(v)->start);
}

oobj oslice::stop_obj() const {
  return oobj(reinterpret_cast<PySliceObject*>(v)->stop);
}



}  // namespace py
