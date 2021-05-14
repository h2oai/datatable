//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <type_traits>
#include "column/cast.h"
#include "csv/toa.h"
#include "python/float.h"
#include "python/int.h"
namespace dt {



template <typename T>
ColumnImpl* CastNumeric_ColumnImpl<T>::clone() const {
  return new CastNumeric_ColumnImpl<T>(stype(), Column(arg_));
}


// Retrieve element of type T, then cast into type V and write to `out`.
//
template <typename T>
template <typename V>
inline bool CastNumeric_ColumnImpl<T>::_get(size_t i, V* out) const {
  T x;
  bool isvalid = arg_.get_element(i, &x);
  *out = static_cast<V>(x);
  return isvalid;
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, int8_t* out) const {
  return _get<int8_t>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, int16_t* out) const {
  return _get<int16_t>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, int32_t* out) const {
  return _get<int32_t>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, int64_t* out) const {
  return _get<int64_t>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, float* out) const {
  return _get<float>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, double* out) const {
  return _get<double>(i, out);
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, CString* out)  const {
  T x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    char* ch = out->prepare_buffer(30);
    char* ch0 = ch;
    toa<T>(&ch, x);
    out->set_size(static_cast<size_t>(ch - ch0));
  }
  return isvalid;
}


template <typename T>
bool CastNumeric_ColumnImpl<T>::get_element(size_t i, py::oobj* out) const {
  T x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    if (std::is_integral<T>::value) *out = py::oint(static_cast<int64_t>(x));
    else                            *out = py::ofloat(static_cast<double>(x));
  }
  return isvalid;
}


template class CastNumeric_ColumnImpl<int8_t>;
template class CastNumeric_ColumnImpl<int16_t>;
template class CastNumeric_ColumnImpl<int32_t>;
template class CastNumeric_ColumnImpl<int64_t>;
template class CastNumeric_ColumnImpl<float>;
template class CastNumeric_ColumnImpl<double>;




}  // namespace dt
