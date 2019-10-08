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

olist::olist() : oobj(nullptr) {}

olist::olist(size_t n) {
  is_list = true;
  v = PyList_New(static_cast<Py_ssize_t>(n));
  if (!v) throw PyError();
}

olist::olist(int n) : olist(static_cast<size_t>(n)) {}

olist::olist(int64_t n) : olist(static_cast<size_t>(n)) {}


olist::olist(const olist& other) : oobj(other) {
  is_list = other.is_list;
}

olist::olist(olist&& other) : oobj(std::move(other)) {
  is_list = other.is_list;
}

olist& olist::operator=(const olist& other) {
  oobj::operator=(other);
  is_list = other.is_list;
  return *this;
}

olist& olist::operator=(olist&& other) {
  oobj::operator=(std::move(other));
  is_list = other.is_list;
  return *this;
}

olist::olist(PyObject* src) : oobj(src) {
  is_list = src && PyList_Check(src);
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



}
