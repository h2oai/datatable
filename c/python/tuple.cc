//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/tuple.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

otuple::otuple() : oobj(nullptr) {}

otuple::otuple(PyObject* v) : oobj(v) {}

otuple::otuple(int n) : otuple(static_cast<int64_t>(n)) {}

otuple::otuple(size_t n) : otuple(static_cast<int64_t>(n)) {}

otuple::otuple(int64_t n) {
  v = PyTuple_New(n);
}

otuple::otuple(const otuple& other) : oobj(other) {}

otuple::otuple(otuple&& other) : oobj(std::move(other)) {}

otuple& otuple::operator=(const otuple& other) {
  oobj::operator=(other);
  return *this;
}

otuple& otuple::operator=(otuple&& other) {
  oobj::operator=(std::move(other));
  return *this;
}



//------------------------------------------------------------------------------
// Element accessors
//------------------------------------------------------------------------------

robj otuple::operator[](int64_t i) const {
  // PyTuple_GET_ITEM returns a borrowed reference
  return robj(PyTuple_GET_ITEM(v, i));
}

robj otuple::operator[](size_t i) const {
  return this->operator[](static_cast<int64_t>(i));
}

robj otuple::operator[](int i) const {
  return this->operator[](static_cast<int64_t>(i));
}

robj rtuple::operator[](size_t i) const {
  return robj(PyTuple_GET_ITEM(v, static_cast<int64_t>(i)));
}


void otuple::set(int64_t i, const _obj& value) {
  // PyTuple_SET_ITEM "steals" a reference to the last argument
  PyTuple_SET_ITEM(v, i, value.to_pyobject_newref());
}

void otuple::set(int64_t i, oobj&& value) {
  PyTuple_SET_ITEM(v, i, std::move(value).release());
}

void otuple::set(size_t i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void otuple::set(size_t i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}

void otuple::set(int i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void otuple::set(int i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}


void otuple::replace(int64_t i, const _obj& value) {
  // PyTuple_SetItem "steals" a reference to the last argument
  PyTuple_SetItem(v, i, value.to_pyobject_newref());
}

void otuple::replace(int64_t i, oobj&& value) {
  PyTuple_SetItem(v, i, std::move(value).release());
}

void otuple::replace(size_t i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void otuple::replace(size_t i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
}

void otuple::replace(int i, const _obj& value) {
  set(static_cast<int64_t>(i), value);
}

void otuple::replace(int i, oobj&& value) {
  set(static_cast<int64_t>(i), std::move(value));
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
