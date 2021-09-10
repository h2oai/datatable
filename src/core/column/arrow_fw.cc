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
#include "column/arrow_fw.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {



ArrowFw_ColumnImpl::ArrowFw_ColumnImpl(size_t nrows, SType stype,
                                       Buffer&& valid, Buffer&& data)
  : Arrow_ColumnImpl(nrows, stype),
    validity_(std::move(valid)),
    data_(std::move(data))
{
  xassert(!validity_ || validity_.size() >= (nrows + 7) / 8);
  xassert(data_.size() == stype_elemsize(stype) * nrows);
}


ColumnImpl* ArrowFw_ColumnImpl::clone() const {
  return new ArrowFw_ColumnImpl(
                nrows(), stype(), Buffer(validity_), Buffer(data_));
}


size_t ArrowFw_ColumnImpl::get_num_data_buffers() const noexcept {
  return 2;
}

Buffer ArrowFw_ColumnImpl::get_data_buffer(size_t i) const {
  return (i == 0)? validity_ : data_;
}




//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

template <typename T>
inline bool ArrowFw_ColumnImpl::_get(size_t  i, T* out) const {
  xassert(i < nrows_);
  xassert(type().can_be_read_as<T>());
  auto validity_data = static_cast<const uint8_t*>(validity_.rptr());
  bool valid = !validity_data || (validity_data[i / 8] & (1 << (i & 7)));
  if (valid) {
    *out = static_cast<const T*>(data_.rptr())[i];
  }
  return valid;
}

bool ArrowFw_ColumnImpl::get_element(size_t i, int8_t* out)  const {
  return _get<int8_t>(i, out);
}

bool ArrowFw_ColumnImpl::get_element(size_t i, int16_t* out)  const {
  return _get<int16_t>(i, out);
}

bool ArrowFw_ColumnImpl::get_element(size_t i, int32_t* out)  const {
  return _get<int32_t>(i, out);
}

bool ArrowFw_ColumnImpl::get_element(size_t i, int64_t* out)  const {
  return _get<int64_t>(i, out);
}

bool ArrowFw_ColumnImpl::get_element(size_t i, float* out)  const {
  return _get<float>(i, out);
}

bool ArrowFw_ColumnImpl::get_element(size_t i, double* out)  const {
  return _get<double>(i, out);
}




}  // namespace dt
