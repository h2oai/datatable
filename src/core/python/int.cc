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
// See https://docs.python.org/3/c-api/long.html
// for the details of Python API
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


// private constructor
oint::oint(const robj& src) : oobj(src) {}



//------------------------------------------------------------------------------
// ovalue<T>
//------------------------------------------------------------------------------

template<>
int8_t oint::ovalue(int* overflow) const {
  if (!v) return GETNA<int8_t>();
  long res = PyLong_AsLongAndOverflow(v, overflow);
  int8_t ires = static_cast<int8_t>(res);
  if (res != ires) {
    *overflow = (res > 0) - (res < 0);
  }
  return *overflow == 0? ires :
         *overflow == 1? std::numeric_limits<int8_t>::max()
                       : -std::numeric_limits<int8_t>::max();
}


template<>
int16_t oint::ovalue(int* overflow) const {
  if (!v) return GETNA<int16_t>();
  long res = PyLong_AsLongAndOverflow(v, overflow);
  int16_t ires = static_cast<int16_t>(res);
  if (res != ires) {
    *overflow = (res > 0) - (res < 0);
  }
  return *overflow == 0? ires :
         *overflow == 1? std::numeric_limits<int16_t>::max()
                       : -std::numeric_limits<int16_t>::max();
}


template<>
int32_t oint::ovalue(int* overflow) const {
  if (!v) return GETNA<int32_t>();
  long res = PyLong_AsLongAndOverflow(v, overflow);
  int32_t ires = static_cast<int32_t>(res);
  if (res != ires) {
    *overflow = (res > 0) - (res < 0);
  }
  return *overflow? std::numeric_limits<int32_t>::max() * (*overflow)
                  : ires;
}


template<>
int64_t oint::ovalue(int* overflow) const {
  if (!v) return GETNA<int64_t>();
  #if LONG_MAX==9223372036854775807
    long res = PyLong_AsLongAndOverflow(v, overflow);
  #else
    long long res = PyLong_AsLongLongAndOverflow(v, overflow);
  #endif
  return *overflow? std::numeric_limits<int64_t>::max() * (*overflow)
                  : static_cast<int64_t>(res);
}


template<>
float oint::ovalue(int* overflow) const {
  if (!v) return GETNA<float>();
  static constexpr double max_float =
    static_cast<double>(std::numeric_limits<float>::max());
  double value = PyLong_AsDouble(v);
  if (value == -1 && PyErr_Occurred()) {
    int sign = _PyLong_Sign(v);
    *overflow = sign;
    return sign > 0 ? std::numeric_limits<float>::infinity()
                    : -std::numeric_limits<float>::infinity();
  } else {
    *overflow = (value > max_float) - (value < -max_float);
    // If value is greater than float_max, this cast should convert it to inf
    return static_cast<float>(value);
  }
}


template<>
double oint::ovalue(int* overflow) const {
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



//------------------------------------------------------------------------------
// xvalue<T>
//------------------------------------------------------------------------------

template<>
int8_t oint::xvalue() const {
  int overflow;
  int8_t res = ovalue<int8_t>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large to convert into `int8`";
  }
  return res;
}


template<>
int16_t oint::xvalue() const {
  int overflow;
  int16_t res = ovalue<int16_t>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large to convert into `int16`";
  }
  return res;
}


template<>
int32_t oint::xvalue() const {
  int overflow;
  int32_t res = ovalue<int32_t>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large to convert into `int32`";
  }
  return res;
}


template<>
int64_t oint::xvalue() const {
  int overflow;
  int64_t res = ovalue<int64_t>(&overflow);
  if (overflow) {
    throw OverflowError() << "Integer is too large to convert into `int64`";
  }
  return res;
}


template<>
size_t oint::xvalue() const {
  if (!v) return size_t(-1);
  if (Py_SIZE(v) < 0) {  // Implementation detail of python `long` object
    throw OverflowError() << "Negative integer cannot be converted to `size_t`";
  }
  size_t res = PyLong_AsSize_t(v);
  if (res == static_cast<size_t>(-1) && PyErr_Occurred()) {
    throw OverflowError() << "Integer is too large to convert into `size_t`";
  }
  return res;
}


template<>
double oint::xvalue() const {
  if (!v) return GETNA<double>();
  double value = PyLong_AsDouble(v);
  if (value == -1 && PyErr_Occurred()) {
    throw OverflowError() << "Integer is too large to convert into `double`";
  }
  return value;
}




//------------------------------------------------------------------------------
// mvalue<T>
//------------------------------------------------------------------------------

template<typename T>
static T masked_value_long(PyObject* v) {
  if (!v) return GETNA<T>();
  unsigned long x = PyLong_AsUnsignedLongMask(v);
  if (x == static_cast<unsigned long>(-1) && PyErr_Occurred()) {
    PyErr_Clear();
    return GETNA<T>();
  }
  return static_cast<T>(x);
}


template<>
int8_t oint::mvalue() const {
  return masked_value_long<int8_t>(v);
}


template<>
int16_t oint::mvalue() const {
  return masked_value_long<int16_t>(v);
}


template<>
int32_t oint::mvalue() const {
  return masked_value_long<int32_t>(v);
}


template<>
int64_t oint::mvalue() const {
  #if LONG_MAX==9223372036854775807
    return masked_value_long<int64_t>(v);
  #else
    if (!v) return GETNA<int64_t>();
    unsigned long long x = PyLong_AsUnsignedLongLongMask(v);
    if (x == static_cast<unsigned long long>(-1) && PyErr_Occurred()) {
      PyErr_Clear();
      return GETNA<int64_t>();
    }
    return static_cast<int64_t>(x);
  #endif
}



}  // namespace py
