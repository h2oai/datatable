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


Float::Float(double v) {
  obj = PyFloat_FromDouble(v);  // new ref
}


Float::Float(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    obj = nullptr;
  } else {
    obj = src;
    if (!PyFloat_Check(src)) {
      throw TypeError() << "Object " << src << " is not a float";
    }
    Py_INCREF(src);
  }
}

Float::Float(const Float& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

Float::Float(Float&& other) : Float() {
  swap(*this, other);
}

Float::~Float() {
  Py_XDECREF(obj);
}


void swap(Float& first, Float& second) noexcept {
  std::swap(first.obj, second.obj);
}


Float Float::fromAnyObject(PyObject* obj) {
  Float res;
  PyObject* num = PyNumber_Float(obj);  // new ref
  if (num) {
    res.obj = num;
  } else {
    PyErr_Clear();
  }
  return res;
}




//------------------------------------------------------------------------------
// Value conversion
//------------------------------------------------------------------------------

template<typename T>
T Float::value() const {
  return obj? static_cast<T>(PyFloat_AS_DOUBLE(obj)) : GETNA<T>();
}



template float  Float::value() const;
template double Float::value() const;

}  // namespace py
