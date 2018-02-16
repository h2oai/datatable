//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/float.h"
#include "utils/exceptions.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

PyyFloat::PyyFloat() : obj(nullptr) {}


PyyFloat::PyyFloat(double v) {
  obj = PyFloat_FromDouble(v);  // new ref
}


PyyFloat::PyyFloat(PyObject* src) {
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

PyyFloat::PyyFloat(const PyyFloat& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

PyyFloat::PyyFloat(PyyFloat&& other) : PyyFloat() {
  swap(*this, other);
}

PyyFloat::~PyyFloat() {
  Py_XDECREF(obj);
}


void swap(PyyFloat& first, PyyFloat& second) noexcept {
  std::swap(first.obj, second.obj);
}


PyyFloat PyyFloat::fromAnyObject(PyObject* obj) {
  PyyFloat res;
  PyObject* num = PyNumber_Float(obj);  // new ref
  if (num) {
    res.obj = num;
  } else {
    PyErr_Clear();
  }
  return res;
}




//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

double PyyFloat::value() const {
  return obj? PyFloat_AS_DOUBLE(obj) : GETNA<double>();
}

