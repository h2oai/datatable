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
#include "python/range.h"
#include "python/int.h"
#include "python/tuple.h"
namespace py {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

orange::orange(int64_t stop) : orange(0, stop, 1) {}


orange::orange(int64_t start, int64_t stop) : orange(start, stop, 1) {}


orange::orange(int64_t start, int64_t stop, int64_t step) {
  otuple args = otuple(py::oint(start), py::oint(stop), py::oint(step));
  v = PyObject_CallObject(reinterpret_cast<PyObject*>(&PyRange_Type),
                          args.to_borrowed_ref());
  if (!v) throw PyError();
}


orange::orange(const robj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

int64_t orange::start() const {
  return get_attr("start").to_int64();
}

int64_t orange::stop() const {
  return get_attr("stop").to_int64();
}

int64_t orange::step() const {
  return get_attr("step").to_int64();
}


bool orange::normalize(
    size_t len, size_t* pstart, size_t* pcount, size_t* pstep) const
{
  return orange::normalize(len, start(), stop(), step(), pstart, pcount, pstep);
}


bool orange::normalize(
    size_t len,
    int64_t istart, int64_t istop, int64_t istep,
    size_t* ostart, size_t* ocount, size_t* ostep)
{
  int64_t ilen = static_cast<int64_t>(len);
  int64_t icount = (istep > 0)? (istop - istart + istep - 1) / istep
                              : (istart - istop - istep - 1) / (-istep);
  if (icount <= 0) {
    *ostart = 0;
    *ocount = 0;
    *ostep = 1;
    return true;
  }

  istop = istart + (icount - 1) * istep;

  if (istart < -ilen || istart >= ilen || istop < -ilen || istop >= ilen ||
      (istart >= 0) != (istop >= 0)) {
    return false;
  }
  if (istart < 0) {
    istart += ilen;
    istop += ilen;
  }

  *ostart = static_cast<size_t>(istart);
  *ocount = static_cast<size_t>(icount);
  *ostep = static_cast<size_t>(istep);
  return true;
}



}  // namespace py
