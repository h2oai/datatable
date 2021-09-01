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
#include "column/arrow_bool.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {



ArrowBool_ColumnImpl::ArrowBool_ColumnImpl(size_t nrows,
                                           Buffer&& valid, Buffer&& data)
  : Arrow_ColumnImpl(nrows, dt::SType::BOOL),
    validity_(std::move(valid)),
    data_(std::move(data))
{
  // Buffer can be padded to 8-byte boundary, causing its size to be
  // slightly bigger than necessary.
  xassert(!validity_ || validity_.size() >= (nrows + 7) / 8);
  xassert(data_.size() >= (nrows + 7) / 8);
}


ColumnImpl* ArrowBool_ColumnImpl::clone() const {
  return new ArrowBool_ColumnImpl(nrows_, Buffer(validity_), Buffer(data_));
}


size_t ArrowBool_ColumnImpl::get_num_data_buffers() const noexcept {
  return 2;
}

Buffer ArrowBool_ColumnImpl::get_data_buffer(size_t i) const {
  return (i == 0)? validity_ : data_;
}




//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

bool ArrowBool_ColumnImpl::get_element(size_t i, int8_t* out)  const {
  xassert(i < nrows_);
  auto validity_arr = static_cast<const uint8_t*>(validity_.rptr());
  bool valid = !validity_arr || (validity_arr[i / 8] & (1 << (i & 7)));
  if (valid) {
    auto data_arr = static_cast<const uint8_t*>(data_.rptr());
    *out = bool(data_arr[i / 8] & (1 << (i & 7)));
  }
  return valid;
}




}  // namespace dt
