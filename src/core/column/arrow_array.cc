//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "column/arrow_array.h"
#include "column/view.h"
namespace dt {


template <typename T>
Type _type_with_child(const Type& t) {
  return (sizeof(T) == 4)? Type::arr32(t) : Type::arr64(t);
}


template <typename T>
ArrowArray_ColumnImpl<T>::ArrowArray_ColumnImpl(
    size_t nrows, size_t nullcount,
    Buffer&& valid, Buffer&& offsets, Column&& child
) : Arrow_ColumnImpl(nrows, _type_with_child<T>(child.type())),
    validity_(std::move(valid)),
    offsets_(std::move(offsets)),
    child_(std::move(child)),
    null_count_(nullcount)
{
  xassert(!validity_ || validity_.size() >= (nrows + 63)/64 * 8);
  xassert(offsets_.size() >= sizeof(T) * (nrows + 1));
  xassert(child_.nrows() >=
          static_cast<size_t>(offsets_.get_element<T>(nrows)));
}


template <typename T>
ColumnImpl* ArrowArray_ColumnImpl<T>::clone() const {
  return new ArrowArray_ColumnImpl<T>(
      nrows_, null_count_, Buffer(validity_), Buffer(offsets_), Column(child_));
}


template <typename T>
size_t ArrowArray_ColumnImpl<T>::n_children() const noexcept {
  return 1;
}


template <typename T>
const Column& ArrowArray_ColumnImpl<T>::child(size_t) const {
  return child_;
}


template <typename T>
size_t ArrowArray_ColumnImpl<T>::num_buffers() const noexcept {
  return 2;
}


template <typename T>
const void* ArrowArray_ColumnImpl<T>::get_buffer(size_t i) const {
  xassert(i <= 1);
  return (i == 0)? validity_.rptr() : offsets_.rptr();
}



template <typename T>
bool ArrowArray_ColumnImpl<T>::get_element(size_t i, Column* out) const {
  xassert(i < nrows_);
  auto validity_data = static_cast<const uint8_t*>(validity_.rptr());
  bool valid = (!validity_data) || (validity_data[i / 8] & (1 << (i & 7)));
  if (valid) {
    auto offsets_data = static_cast<const T*>(offsets_.rptr());
    auto start = static_cast<size_t>(offsets_data[i]);
    auto end = static_cast<size_t>(offsets_data[i + 1]);
    *out = Column(
      new SliceView_ColumnImpl(Column(child_), start, end - start, 1)
    );
  }
  return valid;
}


template <typename T>
size_t ArrowArray_ColumnImpl<T>::null_count() const {
  return null_count_;
}




template class ArrowArray_ColumnImpl<uint32_t>;
template class ArrowArray_ColumnImpl<uint64_t>;


}  // namespace dt
