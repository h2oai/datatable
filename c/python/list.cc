//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/list.h"
#include "utils/assert.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

olist::olist(PyObject* src) : oobj(src) {
  is_list = src && PyList_Check(src);
}

olist::olist(size_t n) {
  is_list = true;
  v = PyList_New(static_cast<Py_ssize_t>(n));
  if (!v) throw PyError();
}

olist::operator bool() const noexcept {
  return v != nullptr;
}



//------------------------------------------------------------------------------
// Element accessors
//------------------------------------------------------------------------------

size_t olist::size() const noexcept {
  return static_cast<size_t>(Py_SIZE(v));
}


obj olist::operator[](int64_t i) const {
  return obj(is_list? PyList_GET_ITEM(v, i)
                    : PyTuple_GET_ITEM(v, i));
}

obj olist::operator[](size_t i) const {
  return this->operator[](static_cast<int64_t>(i));
}

obj olist::operator[](int i) const {
  return this->operator[](static_cast<int64_t>(i));
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
    PyList_SET_ITEM(v, i, value.release());
  } else {
    PyTuple_SET_ITEM(v, i, value.release());
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


}
