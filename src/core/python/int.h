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
#ifndef dt_PYTHON_INT_h
#define dt_PYTHON_INT_h
#include "python/obj.h"

namespace py {


/**
 * C++ wrapper around PyLong_Object (python `int`).
 *
 * Public API
 * ----------
 * ovalue<T>(int* overflow)
 *   Return the value converted into C++ type `T`. If the value cannot be
 *   converted, set the `overflow` flag to +1/-1, and the value returned will
 *   be ±MAX<T>.
 *
 * xvalue<T>()
 *   Return the value converted into C++ type `T`, or throw an overflow
 *   exception if the value cannot be converted into `T`.
 *
 * mvalue<T>()
 *   Similar to the first method, but if the value does not fit into <T>
 *   truncate it using `static_cast<T>`.
 *
 */
class oint : public oobj {
  public:
    oint() = default;
    oint(const oint&) = default;
    oint(oint&&) = default;
    oint& operator=(const oint&) = default;
    oint& operator=(oint&&) = default;

    oint(int32_t n);
    oint(int64_t n);
    oint(size_t n);
    oint(double x);

    template<typename T> T ovalue(int*) const;
    template<typename T> T xvalue() const;
    template<typename T> T mvalue() const;

  private:
    // Private constructor, used from `_obj`. If you need to construct
    // `oint` from `oobj`, use `oobj.to_pyint()` instead.
    oint(const robj&);
    friend class _obj;
};



// Explicit specializations
template<> int8_t  oint::ovalue(int*) const;
template<> int16_t oint::ovalue(int*) const;
template<> int32_t oint::ovalue(int*) const;
template<> int64_t oint::ovalue(int*) const;
template<> size_t  oint::ovalue(int*) const;
template<> float   oint::ovalue(int*) const;
template<> double  oint::ovalue(int*) const;

template<> int8_t  oint::xvalue() const;
template<> int16_t oint::xvalue() const;
template<> int32_t oint::xvalue() const;
template<> int64_t oint::xvalue() const;
template<> size_t  oint::xvalue() const;
template<> double  oint::xvalue() const;

template<> int8_t  oint::mvalue() const;
template<> int16_t oint::mvalue() const;
template<> int32_t oint::mvalue() const;
template<> int64_t oint::mvalue() const;

}  // namespace py

#endif
