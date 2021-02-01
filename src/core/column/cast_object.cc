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
#include <cmath>             // std::isnan
#include <cstring>           // std::memcpy
#include "column/cast.h"
#include "python/_all.h"
namespace dt {


ColumnImpl* CastObject_ColumnImpl::clone() const {
  return new CastObject_ColumnImpl(stype(), Column(arg_));
}

bool CastObject_ColumnImpl::allow_parallel_access() const {
  return false;
}



template <typename V>
inline bool CastObject_ColumnImpl::_get_int(size_t i, V* out) const {
  py::oobj x;
  bool isvalid = arg_.get_element(i, &x);
  if (!isvalid) return false;
  py::oint y = std::move(x).to_pyint_force();
  if (!y) return false;  // Value cannot be cast into int
  *out = y.mvalue<V>();
  return true;
}

bool CastObject_ColumnImpl::get_element(size_t i, int8_t* out) const {
  return _get_int<int8_t>(i, out);
}

bool CastObject_ColumnImpl::get_element(size_t i, int16_t* out) const {
  return _get_int<int16_t>(i, out);
}

bool CastObject_ColumnImpl::get_element(size_t i, int32_t* out) const {
  return _get_int<int32_t>(i, out);
}

bool CastObject_ColumnImpl::get_element(size_t i, int64_t* out) const {
  return _get_int<int64_t>(i, out);
}



template <typename V>
inline bool CastObject_ColumnImpl::_get_float(size_t i, V* out) const {
  py::oobj x;
  bool isvalid = arg_.get_element(i, &x);
  if (!isvalid) return false;
  py::ofloat y = std::move(x).to_pyfloat_force();
  if (!y) return false;  // Value cannot be cast into int
  *out = y.value<V>();
  return !std::isnan(*out);
}


bool CastObject_ColumnImpl::get_element(size_t i, float* out) const {
  return _get_float<float>(i, out);
}


bool CastObject_ColumnImpl::get_element(size_t i, double* out) const {
  return _get_float<double>(i, out);
}


bool CastObject_ColumnImpl::get_element(size_t i, CString* out)  const {
  py::oobj x;
  bool isvalid = arg_.get_element(i, &x);
  if (!isvalid) return false;
  py::ostring y = std::move(x).to_pystring_force();
  if (!y) return false;

  // In most cases the returned py::ostring `y` is owned externally
  // (its refcount > 1), which means it should be safe to return the
  // undeflying CString as a simple pointer. The pointer shall survive
  // until the upstream owner is alive. In practice it means that the
  // returned CString should be valid provided that it is consumed
  // immediately.
  // In rare cases, however, the returned py::ostring `y` is only
  // owned by this function (for example this may happen if oobj is
  // generated on-the-fly). In such cases we need to copy the string
  // into the buffer of `out`, so that it wouldn't be destroyed
  // prematurely.
  if (y.get_refcount() > 1) {
    *out = y.to_cstring();
  }
  else {
    CString z = y.to_cstring();
    char* ch = out->prepare_buffer(z.size());
    std::memcpy(ch, z.data(), z.size());
  }
  return true;
}





}  // namespace dt
