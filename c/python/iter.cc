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
#include "python/iter.h"
namespace py {


//------------------------------------------------------------------------------
// oiter
//------------------------------------------------------------------------------

oiter::oiter(PyObject* src) : oobj(PyObject_GetIter(src)) {}



iter_iterator oiter::begin() const noexcept {
  return iter_iterator(v);
}


iter_iterator oiter::end() const noexcept {
  return iter_iterator(nullptr);
}


size_t oiter::size() const noexcept {
  size_t len = static_cast<size_t>(-1);
  try {
    // get_attr() may throw an exception if there is no such attribute
    oobj method = get_attr("__length_hint__");
    PyObject* mptr = method.to_borrowed_ref();
    PyObject* res = PyObject_CallObject(mptr, nullptr);  // new ref
    if (res) {
      long long t = PyLong_AsLongLong(res);
      Py_XDECREF(res);
      len = static_cast<size_t>(t);
    } else {
      PyErr_Clear();
    }
  } catch (...) {}
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
    iter = py::oobj(nullptr);
    next_value = py::oobj(nullptr);
  }
}



}  // namespace py
