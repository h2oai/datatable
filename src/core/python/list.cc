//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "python/list.h"
#include "python/python.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/macros.h"
namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

olist::olist(size_t n) {
  is_list = true;
  v = PyList_New(static_cast<Py_ssize_t>(n));
  if (!v) throw PyError();
}

olist::olist(int n) : olist(static_cast<size_t>(n)) {}

olist::olist(int64_t n) : olist(static_cast<size_t>(n)) {}


olist::olist(const robj& src) : oobj(src) {
  is_list = v && PyList_Check(v);
}


rlist::rlist(const robj& src) : robj(src) {}

rlist rlist::unchecked(const robj& src) {
  return rlist(src);
}



//------------------------------------------------------------------------------
// Element accessors
//------------------------------------------------------------------------------

robj olist::operator[](int64_t i) const {
  return robj(is_list? PyList_GET_ITEM(v, i)
                     : PyTuple_GET_ITEM(v, i));
}

robj olist::operator[](size_t i) const {
  return this->operator[](static_cast<int64_t>(i));
}

robj olist::operator[](int i) const {
  return this->operator[](static_cast<int64_t>(i));
}

robj rlist::operator[](int64_t i) const {
  return robj(PyList_GET_ITEM(v, i));
}

robj rlist::operator[](size_t i) const {
  return robj(PyList_GET_ITEM(v, static_cast<int64_t>(i)));
}



void olist::set(int64_t i, const _obj& value) {
  if (is_list) {
    PyList_SET_ITEM(v, i, value.to_pyobject_newref());
  } else {
    PyTuple_SET_ITEM(v, i, value.to_pyobject_newref());
  }
}

void olist::set(int64_t i, oobj&& value) {
  if (is_list) {
    PyList_SET_ITEM(v, i, std::move(value).release());
  } else {
    PyTuple_SET_ITEM(v, i, std::move(value).release());
  }
}

void olist::set(size_t i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void olist::set(size_t i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}

void olist::set(int i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void olist::set(int i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}

void olist::append(const _obj& value) {
  xassert(value);
  int ret = PyList_Append(v, value.to_borrowed_ref());
  if (ret == -1) throw PyError();
}



//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

olist::operator bool() const noexcept {
  return v != nullptr;
}

size_t olist::size() const noexcept {
  return static_cast<size_t>(Py_SIZE(v));
}


size_t rlist::size() const noexcept {
  return static_cast<size_t>(Py_SIZE(v));
}



}
