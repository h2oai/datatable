//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
  : Virtual_ColumnImpl(nrows, stype),
    validity_(std::move(valid)),
    offsets_(std::move(offsets)),
    strdata_(std::move(data))
{
  xassert(validity_.size() == (nrows + 7) / 8);
  xassert(offsets_.size() == stype_elemsize(stype) * (nrows + 1));
  xassert(compatible_type<T>(stype_));
}


template <typename T>
ColumnImpl* ArrowStr_ColumnImpl<T>::clone() const {
  return new ArrowStr_ColumnImpl(
      nrows_, stype_, Buffer(validity_), Buffer(offsets_), Buffer(strdata_));
}


template <typename T>
size_t ArrowStr_ColumnImpl<T>::n_children() const noexcept {
  return 0;
}




//------------------------------------------------------------------------------
// Element access
//------------------------------------------------------------------------------

template <typename T>
bool ArrowStr_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  xassert(i < nrows_);
  bool valid = static_cast<const uint8_t*>(validity_.rptr())[i / 8]
               & (1 << (i & 7));
  if (valid) {
    T start = static_cast<const T*>(offsets_.rptr())[i];
    T end = static_cast<const T*>(offsets_.rptr())[i + 1];
    xassert(end >= start);
    out->set(static_cast<const char*>(strdata_.rptr()) + start, end - start);
  }
  return valid;
}




}  // namespace dt
