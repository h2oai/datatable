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

PyyLong::PyyLong(int32_t n) {
  obj = PyLong_FromLong(n);
}

PyyLong::PyyLong(int64_t n) {
  obj = PyLong_FromLongLong(n);
}

PyyLong::PyyLong(size_t n) {
  obj = PyLong_FromSize_t(n);
}

PyyLong::PyyLong(double x) {
  obj = PyLong_FromDouble(x);
}

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

PyyLong::operator PyObj() const & {
  return PyObj(obj);
}

PyyLong::operator PyObj() && {
  PyObject* t = obj;
  obj = nullptr;
  return PyObj::fromPyObjectNewRef(t);
}


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


template<> float PyyLong::value<float>(int* overflow) const {
  static constexpr double max_float =
    static_cast<double>(std::numeric_limits<float>::max());
  double value = PyLong_AsDouble(obj);
  if (value == -1 && PyErr_Occurred()) {
    int sign = _PyLong_Sign(obj);
    *overflow = 1;
    return sign > 0 ? std::numeric_limits<float>::infinity()
                    : -std::numeric_limits<float>::infinity();
  } else {
    *overflow = (value > max_float || value < -max_float);
    // If value is greater than float_max, this cast should convert it to inf
    return static_cast<float>(value);
  }
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


template<> long long int PyyLong::masked_value() const {
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

template int8_t  PyyLong::value() const;
template int16_t PyyLong::value() const;
template int32_t PyyLong::value() const;
template int64_t PyyLong::value() const;
template float   PyyLong::value() const;
template double  PyyLong::value() const;

template signed char  PyyLong::value(int*) const;
template short int    PyyLong::value(int*) const;
template int          PyyLong::value(int*) const;

template signed char   PyyLong::masked_value() const;
template short int     PyyLong::masked_value() const;
template int           PyyLong::masked_value() const;
template long int      PyyLong::masked_value() const;
