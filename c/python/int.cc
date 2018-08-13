//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/int.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

Int::Int() : obj(nullptr) {}

Int::Int(int32_t n) {
  obj = PyLong_FromLong(n);
}

Int::Int(int64_t n) {
  obj = PyLong_FromLongLong(n);
}

Int::Int(size_t n) {
  obj = PyLong_FromSize_t(n);
}

Int::Int(double x) {
  obj = PyLong_FromDouble(x);
}

Int::Int(PyObject* src) {
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

Int::Int(const Int& other) {
  obj = other.obj;
  Py_INCREF(obj);
}

Int::Int(Int&& other) : Int() {
  swap(*this, other);
}

Int::~Int() {
  Py_XDECREF(obj);
}


void swap(Int& first, Int& second) noexcept {
  std::swap(first.obj, second.obj);
}


Int Int::fromAnyObject(PyObject* obj) {
  Int res;
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

Int::operator py::oobj() && {
  PyObject* t = obj;
  obj = nullptr;
  return py::oobj::from_new_reference(t);
}

PyObject* Int::release() {
  PyObject* t = obj;
  obj = nullptr;
  return t;
}



template<> long Int::value<long>(int* overflow) const {
  if (!obj) return GETNA<long>();
  long value = PyLong_AsLongAndOverflow(obj, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long>::max()
                          : -std::numeric_limits<long>::max();
  }
  return value;
}


template<> long long Int::value<long long>(int* overflow) const {
  if (!obj) return GETNA<long long>();
  long long value = PyLong_AsLongLongAndOverflow(obj, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long long>::max()
                          : -std::numeric_limits<long long>::max();
  }
  return value;
}


template<> double Int::value<double>(int* overflow) const {
  if (!obj) return GETNA<double>();
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


template<> float Int::value<float>(int* overflow) const {
  if (!obj) return GETNA<float>();
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


template<typename T> T Int::value(int* overflow) const {
  if (!obj) return GETNA<T>();
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


template<typename T> T Int::value() const {
  if (!obj) return GETNA<T>();
  int overflow;
  T res = value<T>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large for " << typeid(T).name();
  }
  return res;
}


template<> long long int Int::masked_value() const {
  if (!obj) return GETNA<long long int>();
  unsigned long long x = PyLong_AsUnsignedLongLongMask(obj);
  if (x == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return std::numeric_limits<long long>::min();  // NA
  }
  return static_cast<long long>(x);
}


template<typename T> T Int::masked_value() const {
  if (!obj) return GETNA<T>();
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

template int8_t  Int::value() const;
template int16_t Int::value() const;
template int32_t Int::value() const;
template int64_t Int::value() const;
template float   Int::value() const;
template double  Int::value() const;

template signed char  Int::value(int*) const;
template short int    Int::value(int*) const;
template int          Int::value(int*) const;

template signed char   Int::masked_value() const;
template short int     Int::masked_value() const;
template int           Int::masked_value() const;
template long int      Int::masked_value() const;


}  // namespace py
