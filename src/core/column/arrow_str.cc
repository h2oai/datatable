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
#include "column/arrow_str.h"
#include "cstring.h"
#include "stype.h"
#include "utils/assert.h"
namespace dt {


template <typename T>
ArrowStr_ColumnImpl<T>::ArrowStr_ColumnImpl(
    size_t nrows, SType stype, Buffer&& valid, Buffer&& offsets, Buffer&& data
)
  : Arrow_ColumnImpl(nrows, stype),
    validity_(std::move(valid)),
    offsets_(std::move(offsets)),
    strdata_(std::move(data))
{
  xassert(!validity_ || validity_.size() >= (nrows + 7) / 8);
  xassert(offsets_.size() >= sizeof(T) * (nrows + 1));
  xassert(stype_elemsize(stype) == sizeof(T));
}


template <typename T>
ColumnImpl* ArrowStr_ColumnImpl<T>::clone() const {
  return new ArrowStr_ColumnImpl(
      nrows(), stype(), Buffer(validity_), Buffer(offsets_), Buffer(strdata_));
}

template <typename T>
size_t ArrowStr_ColumnImpl<T>::num_buffers() const noexcept {
  return 3;
}

template <typename T>
const void* ArrowStr_ColumnImpl<T>::get_buffer(size_t i) const {
  xassert(i < 3);
  return (i == 0)? validity_.rptr() :
         (i == 1)? offsets_.rptr() :
         (i == 2)? strdata_.rptr() : nullptr;
}



//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

template <typename T>
bool ArrowStr_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  xassert(i < nrows_);
  auto validity_data = static_cast<const uint8_t*>(validity_.rptr());
  bool valid = (!validity_data) || (validity_data[i / 8] & (1 << (i & 7)));
  if (valid) {
    T start = static_cast<const T*>(offsets_.rptr())[i];
    T end = static_cast<const T*>(offsets_.rptr())[i + 1];
    xassert(end >= start);
    out->set(static_cast<const char*>(strdata_.rptr()) + start, end - start);
  }
  return valid;
}



template class ArrowStr_ColumnImpl<uint32_t>;
template class ArrowStr_ColumnImpl<uint64_t>;


}  // namespace dt
