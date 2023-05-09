//------------------------------------------------------------------------------
// Copyright 2021-2023 H2O.ai
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
#include <cmath>             // std::ismath
#include <cstdlib>           // std::strtod
#include "csv/toa.h"
#include "column/cast.h"
#include "python/date.h"
#include "python/string.h"
namespace dt {



ColumnImpl* CastDate32_ColumnImpl::clone() const {
  return new CastDate32_ColumnImpl(stype(), Column(arg_));
}



//------------------------------------------------------------------------------
// Converters
//------------------------------------------------------------------------------

template <typename T>
inline bool CastDate32_ColumnImpl::_get(size_t i, T* out) const {
  int32_t x;
  bool isvalid = arg_.get_element(i, &x);
  *out = static_cast<T>(x);
  return isvalid;
}


bool CastDate32_ColumnImpl::get_element(size_t i, int8_t* out) const {
  return _get<int8_t>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, int16_t* out) const {
  return _get<int16_t>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, int32_t* out) const {
  return _get<int32_t>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, int64_t* out) const {
  return _get<int64_t>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, float* out) const {
  return _get<float>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, double* out) const {
  return _get<double>(i, out);
}


bool CastDate32_ColumnImpl::get_element(size_t i, CString* out) const {
  int32_t x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    char* ch = out->prepare_buffer(14);
    char* ch0 = ch;
    date32_toa(&ch, x);
    out->set_size(static_cast<size_t>(ch - ch0));
  }
  return isvalid;
}


bool CastDate32_ColumnImpl::get_element(size_t i, py::oobj* out) const {
  int32_t x;
  bool isvalid = arg_.get_element(i, &x);
  if (isvalid) {
    *out = py::odate(x);
  }
  return isvalid;
}




}  // namespace dt
