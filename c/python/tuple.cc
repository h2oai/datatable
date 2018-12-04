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
#include "python/tuple.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

otuple::otuple(size_t n) {
  v = PyTuple_New(static_cast<Py_ssize_t>(n));
}

otuple::otuple(const robj& src) : oobj(src) {}
rtuple::rtuple(const robj& src) : robj(src) {}

rtuple rtuple::unchecked(const robj& src) {
  return rtuple(src);
}



//------------------------------------------------------------------------------
// Element accessors
//------------------------------------------------------------------------------

robj otuple::operator[](size_t i) const {
  return robj(PyTuple_GET_ITEM(v, static_cast<int64_t>(i)));
  // return this->operator[](static_cast<int64_t>(i));
}

robj rtuple::operator[](size_t i) const {
  return robj(PyTuple_GET_ITEM(v, static_cast<int64_t>(i)));
}


void otuple::set(size_t i, const _obj& value) {
  // PyTuple_SET_ITEM "steals" a reference to the last argument
  PyTuple_SET_ITEM(v, static_cast<Py_ssize_t>(i), value.to_pyobject_newref());
}

void otuple::set(size_t i, oobj&& value) {
  PyTuple_SET_ITEM(v, static_cast<Py_ssize_t>(i), std::move(value).release());
}


void otuple::replace(size_t i, const _obj& value) {
  // PyTuple_SetItem "steals" a reference to the last argument
  PyTuple_SetItem(v, static_cast<Py_ssize_t>(i), value.to_pyobject_newref());
}

void otuple::replace(size_t i, oobj&& value) {
  PyTuple_SetItem(v, static_cast<Py_ssize_t>(i), std::move(value).release());
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

size_t otuple::size() const noexcept {
  return static_cast<size_t>(Py_SIZE(v));
}

size_t rtuple::size() const noexcept {
  return static_cast<size_t>(Py_SIZE(v));
}


}  // namespace py
