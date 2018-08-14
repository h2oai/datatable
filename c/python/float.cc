//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/float.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

Float::Float() : obj(nullptr) {}

Float::Float(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    obj = nullptr;
  } else {
    obj = src;
    if (!PyFloat_Check(src)) {
      throw TypeError() << "Object " << src << " is not a float";
    }
  }
}

Float::Float(const Float& other) {
  obj = other.obj;
}



oFloat::oFloat() {}

oFloat::oFloat(double v) {
  obj = PyFloat_FromDouble(v);  // new ref
}

oFloat::oFloat(PyObject* src) : Float(src) {
  Py_XINCREF(obj);
}

oFloat::oFloat(const oFloat& other) {
  obj = other.obj;
  Py_XINCREF(obj);
}

oFloat::oFloat(oFloat&& other) : oFloat() {
  swap(*this, other);
}

oFloat oFloat::_from_pyobject_no_checks(PyObject* v) {
  oFloat res;
  res.obj = v;
  return res;
}

oFloat::~oFloat() {
  Py_XDECREF(obj);
}


void swap(Float& first, Float& second) noexcept {
  std::swap(first.obj, second.obj);
}



//------------------------------------------------------------------------------
// Value conversions
//------------------------------------------------------------------------------

template<typename T>
T Float::value() const {
  if (!obj) return GETNA<T>();
  return static_cast<T>(PyFloat_AS_DOUBLE(obj));
}



template float  Float::value() const;
template double Float::value() const;

}  // namespace py
