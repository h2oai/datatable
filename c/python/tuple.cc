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
#include "utils/assert.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

otuple::otuple(size_t n) {
  v = PyTuple_New(static_cast<Py_ssize_t>(n));
}

otuple::otuple(std::initializer_list<oobj> list) : otuple(list.size()) {
  size_t i = 0;
  for (const oobj& item: list) {
    set(i++, item);
  }
}

otuple::otuple(const _obj& o1) : otuple(1) {
  set(0, o1);
}

otuple::otuple(const _obj& o1, const _obj& o2) : otuple(2) {
  set(0, o1);
  set(1, o2);
}

otuple::otuple(const _obj& o1, const _obj& o2, const _obj& o3) : otuple(3) {
  set(0, o1);
  set(1, o2);
  set(2, o3);
}

otuple::otuple(oobj&& o1) : otuple(1) {
  set(0, std::move(o1));
}

otuple::otuple(oobj&& o1, oobj&& o2) : otuple(2) {
  set(0, std::move(o1));
  set(1, std::move(o2));
}

otuple::otuple(oobj&& o1, oobj&& o2, oobj&& o3) : otuple(3) {
  set(0, std::move(o1));
  set(1, std::move(o2));
  set(2, std::move(o3));
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
  // Ensure `v->ob_refcnt == 1`, otherwise a call to `PyTuple_SetItem()`
  // will result in a `SystemError`.
  make_editable();

  // PyTuple_SetItem "steals" a reference to the last argument
  PyTuple_SetItem(v, static_cast<Py_ssize_t>(i), value.to_pyobject_newref());
}


void otuple::replace(size_t i, oobj&& value) {
  // Ensure `v->ob_refcnt == 1`, otherwise a call to `PyTuple_SetItem()`
  // will result in a `SystemError`.
  make_editable();

  PyTuple_SetItem(v, static_cast<Py_ssize_t>(i), std::move(value).release());
}


/**
 * When the reference count of `v` is one, this method is a noop.
 * Otherwise, it will replace `v` with its copy, ensuring that the
 * reference count for the new object is one. This method is useful
 * when we need to replace values in a tuple that has the reference count
 * greater than one. If a copy of `v` is not made in such a case,
 a call to `PyTuple_SetItem()` will resulst in a `SystemError`.
 */
void otuple::make_editable() {
  if (v->ob_refcnt == 1) return;
  PyObject* v_new = PyTuple_GetSlice(v, 0, PyTuple_Size(v)); // new ref
  if (Py_TYPE(v) != Py_TYPE(v_new)) {
    // When `v` is a namedtuple, we need to adjust python type for `v_new`.
    // This is because `PyTuple_GetSlice()` always returns a regular tuple.
    PyTypeObject* v_type = Py_TYPE(v);
    Py_TYPE(v_new) = v_type;
    Py_INCREF(v_type);
  }
  Py_SETREF(v, v_new);
  xassert(v->ob_refcnt == 1);
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
