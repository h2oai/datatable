//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_LONG_h
#define dt_PYTHON_LONG_h
#include <Python.h>
#include "utils/pyobj.h"



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
class PyyLong {
  private:
    PyObject* obj;

  public:
    PyyLong();
    PyyLong(int32_t n);
    PyyLong(int64_t n);
    PyyLong(size_t n);
    PyyLong(double x);
    PyyLong(PyObject*);
    PyyLong(const PyyLong&);
    PyyLong(PyyLong&&);
    ~PyyLong();

    template<typename T> T value() const;
    template<typename T> T value(int* overflow) const;
    template<typename T> T masked_value() const;

    operator PyObj() const &;
    operator PyObj() &&;

    static PyyLong fromAnyObject(PyObject*);
    friend void swap(PyyLong& first, PyyLong& second) noexcept;
};



// Explicit specializations
template<> float     PyyLong::value<float>(int*) const;
template<> double    PyyLong::value<double>(int*) const;
template<> long      PyyLong::value<long>(int*) const;
template<> long long PyyLong::value<long long>(int*) const;
template<> long long PyyLong::masked_value<long long>() const;

// Forward-declare explicit instantiations
extern template int8_t  PyyLong::value() const;
extern template int16_t PyyLong::value() const;
extern template int32_t PyyLong::value() const;
extern template int64_t PyyLong::value() const;
extern template float   PyyLong::value() const;
extern template double  PyyLong::value() const;

extern template int8_t  PyyLong::value(int*) const;
extern template int16_t PyyLong::value(int*) const;
extern template int32_t PyyLong::value(int*) const;
extern template int64_t PyyLong::value(int*) const;

extern template int8_t  PyyLong::masked_value() const;
extern template int16_t PyyLong::masked_value() const;
extern template int32_t PyyLong::masked_value() const;
extern template int64_t PyyLong::masked_value() const;

#endif
