//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "python/iter.h"
#include "python/python.h"
namespace py {


//------------------------------------------------------------------------------
// oiter
//------------------------------------------------------------------------------

oiter::oiter(PyObject* src) : oobj(PyObject_GetIter(src)) {}



iter_iterator oiter::begin() const {
  return iter_iterator(v);
}


iter_iterator oiter::end() const {
  return iter_iterator(nullptr);
}


size_t oiter::size() const noexcept {
  size_t len = size_t(-1);
  try {
    // See https://docs.python.org/3/reference/datamodel.html#object.__len__
    // The .__len__() method is required to return a non-negative integer.
    if (has_attr("__len__")) {
      oobj res = get_attr("__len__").call();
      if (res.is_int()) {
        len = res.to_size_t();
      }
    }
    // The .__length_hint__() method must return either a non-negative integer,
    // or the NotImplemented value.
    else if (has_attr("__length_hint__")) {
      oobj res = get_attr("__length_hint__").call();
      if (res.is_int()) {
        len = res.to_size_t();
      }
    }
  } catch (...) {
    // If either .__len__() or .__length_hint__() throws an exception,
    // we'll catch it here (since this method is declared noexcept).
    PyErr_Clear();
  }
  return len;
}




//------------------------------------------------------------------------------
// iter_iterator
//------------------------------------------------------------------------------

iter_iterator::iter_iterator(PyObject* d)
  : iter(d), next_value(nullptr)
{
  advance();
}

iter_iterator& iter_iterator::operator++() {
  advance();
  return *this;
}

py::robj iter_iterator::operator*() const {
  return next_value;
}

bool iter_iterator::operator==(const iter_iterator& other) const {
  return (iter == other.iter);
}

bool iter_iterator::operator!=(const iter_iterator& other) const {
  return (iter != other.iter);
}

void iter_iterator::advance() {
  if (!iter) return;
  PyObject* res = PyIter_Next(iter.to_borrowed_ref());
  if (res) {
    next_value = py::oobj::from_new_reference(res);
  } else {
    if (PyErr_Occurred()) throw PyError();
    iter = py::oobj(nullptr);
    next_value = py::oobj(nullptr);
  }
}



}  // namespace py
