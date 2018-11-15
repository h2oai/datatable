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
#include <limits>
#include "python/int.h"
#include "utils/exceptions.h"

namespace py {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

oint::oint(int32_t n) {
  v = PyLong_FromLong(n);
}

oint::oint(int64_t n) {
  #if LONG_MAX==9223372036854775807
    v = PyLong_FromLong(n);
  #elif LLONG_MAX==9223372036854775807
    v = PyLong_FromLongLong(n);
  #else
    #error "Cannot determine size of `long`"
  #endif
}

oint::oint(size_t n) {
  v = PyLong_FromSize_t(n);
}

oint::oint(double x) {
  v = PyLong_FromDouble(x);
}


// private constructors
oint::oint(const obj& src) : oobj(src) {}
oint::oint(const oobj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// Value conversions
//------------------------------------------------------------------------------

template<> long oint::value<long>(int* overflow) const {
  if (!v) return GETNA<long>();
  long value = PyLong_AsLongAndOverflow(v, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long>::max()
                          : -std::numeric_limits<long>::max();
  }
  return value;
}


template<> long long oint::value<long long>(int* overflow) const {
  if (!v) return GETNA<int64_t>();
  long long value = PyLong_AsLongLongAndOverflow(v, overflow);
  if (*overflow) {
    value = *overflow > 0 ? std::numeric_limits<long long>::max()
                          : -std::numeric_limits<long long>::max();
  }
  return value;
}


template<> double oint::value<double>(int* overflow) const {
  if (!v) return GETNA<double>();
  double value = PyLong_AsDouble(v);
  if (value == -1 && PyErr_Occurred()) {
    int sign = _PyLong_Sign(v);
    value = sign > 0 ? std::numeric_limits<double>::infinity()
                     : -std::numeric_limits<double>::infinity();
    *overflow = 1;
  } else {
    *overflow = 0;
  }
  return value;
}


template<> float oint::value<float>(int* overflow) const {
  if (!v) return GETNA<float>();
  static constexpr double max_float =
    static_cast<double>(std::numeric_limits<float>::max());
  double value = PyLong_AsDouble(v);
  if (value == -1 && PyErr_Occurred()) {
    int sign = _PyLong_Sign(v);
    *overflow = 1;
    return sign > 0 ? std::numeric_limits<float>::infinity()
                    : -std::numeric_limits<float>::infinity();
  } else {
    *overflow = (value > max_float || value < -max_float);
    // If value is greater than float_max, this cast should convert it to inf
    return static_cast<float>(value);
  }
}


template<typename T> T oint::value(int* overflow) const {
  if (!v) return GETNA<T>();
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


template<typename T> T oint::value() const {
  if (!v) return GETNA<T>();
  int overflow;
  T res = value<T>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large for " << typeid(T).name();
  }
  return res;
}


template<> long long int oint::masked_value() const {
  if (!v) return GETNA<int64_t>();
  unsigned long long x = PyLong_AsUnsignedLongLongMask(v);
  if (x == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return std::numeric_limits<long long>::min();  // NA
  }
  return static_cast<long long>(x);
}


template<typename T> T oint::masked_value() const {
  if (!v) return GETNA<T>();
  unsigned long x = PyLong_AsUnsignedLongMask(v);
  if (x == static_cast<unsigned long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return GETNA<T>();
  }
  return static_cast<T>(x);
}




//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

template int8_t  oint::value() const;
template int16_t oint::value() const;
template int32_t oint::value() const;
template int64_t oint::value() const;
template float   oint::value() const;
template double  oint::value() const;

template signed char  oint::value(int*) const;
template short int    oint::value(int*) const;
template int          oint::value(int*) const;

template signed char   oint::masked_value() const;
template short int     oint::masked_value() const;
template int           oint::masked_value() const;
template long int      oint::masked_value() const;


}  // namespace py
