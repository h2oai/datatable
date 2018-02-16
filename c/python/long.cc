//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/long.h"
#include "utils/exceptions.h"



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

PyyLong::PyyLong() : obj(nullptr) {}


PyyLong::PyyLong(PyObject* src) {
  if (!src) throw PyError();
  if (src == Py_None) {
    obj = nullptr;
  } else {
    obj = src;
    if (!PyLong_Check(src)) {
      throw TypeError() << "Object " << src << " is not an integer";
    }
    Py_INCREF(src);
  }
}

PyyLong::PyyLong(const PyyLong& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

PyyLong::PyyLong(PyyLong&& other) : PyyLong() {
  swap(*this, other);
}

PyyLong::~PyyLong() {
  Py_XDECREF(obj);
}


void swap(PyyLong& first, PyyLong& second) noexcept {
  std::swap(first.obj, second.obj);
}


PyyLong PyyLong::fromAnyObject(PyObject* obj) {
  PyyLong res;
  PyObject* num = PyNumber_Long(obj);  // new ref
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


template<> long PyyLong::value<long>(int* overflow) const {
  long value = PyLong_AsLongAndOverflow(obj, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long>::max()
                          : -std::numeric_limits<long>::max();
  }
  return value;
}


template<> long long PyyLong::value<long long>(int* overflow) const {
  long long value = PyLong_AsLongLongAndOverflow(obj, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long long>::max()
                          : -std::numeric_limits<long long>::max();
  }
  return value;
}


template<> double PyyLong::value<double>(int* overflow) const {
  double value = PyLong_AsDouble(obj);
  if (value == -1 && PyErr_Occurred()) {
    int sign = _PyLong_Sign(obj);
    value = sign > 0 ? std::numeric_limits<double>::infinity()
                     : -std::numeric_limits<double>::infinity();
    *overflow = 1;
  } else {
    *overflow = 0;
  }
  return value;
}


template<typename T> T PyyLong::value(int* overflow) const {
  constexpr T MAX = std::numeric_limits<T>::max();
  long x = value<long>(overflow);
  if (x > MAX) {
    *overflow = 1;
    return MAX;
  }
  if (x < -MAX) {
    *overflow = 1;
    return -MAX;
  }
  return static_cast<T>(x);
}


template<typename T> T PyyLong::value() const {
  int overflow;
  T res = value<T>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large for " << typeid(T).name();
  }
  return res;
}


template<> long long PyyLong::masked_value() const {
  unsigned long long x = PyLong_AsUnsignedLongLongMask(obj);
  if (x == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return std::numeric_limits<long long>::min();  // NA
  }
  return static_cast<long long>(x);
}


template<typename T> T PyyLong::masked_value() const {
  unsigned long x = PyLong_AsUnsignedLongMask(obj);
  if (x == static_cast<unsigned long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return GETNA<T>();
  }
  return static_cast<T>(x);
}



//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

template int8_t  PyyLong::value<int8_t >() const;
template int16_t PyyLong::value<int16_t>() const;
template int32_t PyyLong::value<int32_t>() const;
template int64_t PyyLong::value<int64_t>() const;
template double  PyyLong::value<double >() const;
template int8_t  PyyLong::value<int8_t >(int*) const;
template int16_t PyyLong::value<int16_t>(int*) const;
template int32_t PyyLong::value<int32_t>(int*) const;
template int8_t  PyyLong::masked_value() const;
template int16_t PyyLong::masked_value() const;
template int32_t PyyLong::masked_value() const;
