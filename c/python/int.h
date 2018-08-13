//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_INT_h
#define dt_PYTHON_INT_h
#include <Python.h>
#include "python/obj.h"

namespace py {


/**
 * C++ wrapper around PyLong_Object (python `int` object).
 *
 * Public API
 * ----------
 * value<T>()
 *   Return value as an integral T or double type. If the value cannot be
 *   represented as T, an overflow exception will be thrown.
 *
 * value<T>(int* overflow)
 *   Similar to the previous method, but if the value cannot be represented
 *   as T, sets the `overflow` flag to 1 (or -1) and returns the ±MAX<T>
 *   value depending on the sign of the underlying object.
 *
 * masked_value<T>()
 *   Similar to the first method, but if the value does not fit into <T>
 *   truncate it using `static_cast<T>`.
 *
 */
class Int {  // capitalized, because lowercase `int` is a reserved word
  protected:
    PyObject* obj;

  public:
    Int();
    Int(PyObject*);
    Int(const Int&);
    friend void swap(Int& first, Int& second) noexcept;

    template<typename T> T value() const;
    template<typename T> T value(int* overflow) const;
    template<typename T> T masked_value() const;
};


class oInt : public Int {
  public:
    oInt();
    oInt(int32_t n);
    oInt(int64_t n);
    oInt(size_t n);
    oInt(double x);
    oInt(PyObject*);
    oInt(const oInt&);
    oInt(oInt&&);
    oInt(oobj&&);
    ~oInt();

    static oInt _from_pyobject_no_checks(PyObject* v);
    friend oobj::oobj(oInt&&);
};


// Explicit specializations
template<> float     Int::value<float>(int*) const;
template<> double    Int::value<double>(int*) const;
template<> long      Int::value<long>(int*) const;
template<> long long Int::value<long long>(int*) const;
template<> long long Int::masked_value<long long>() const;

// Forward-declare explicit instantiations
extern template int8_t  Int::value() const;
extern template int16_t Int::value() const;
extern template int32_t Int::value() const;
extern template int64_t Int::value() const;
extern template float   Int::value() const;
extern template double  Int::value() const;

extern template int8_t  Int::value(int*) const;
extern template int16_t Int::value(int*) const;
extern template int32_t Int::value(int*) const;
extern template int64_t Int::value(int*) const;

extern template int8_t  Int::masked_value() const;
extern template int16_t Int::masked_value() const;
extern template int32_t Int::masked_value() const;
extern template int64_t Int::masked_value() const;


}  // namespace py

#endif
