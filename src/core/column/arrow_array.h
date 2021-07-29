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
#ifndef dt_COLUMN_ARROW_ARRAY_h
#define dt_COLUMN_ARROW_ARRAY_h
#include "column/arrow.h"
namespace dt {


template <typename T>
class ArrowArray_ColumnImpl : public Arrow_ColumnImpl {
  private:
    Buffer validity_;
    Buffer offsets_;
    Column child_;

  public:
    ArrowArray_ColumnImpl(
        size_t nrows, Buffer&& valid, Buffer&& offsets, Column&& child);

    ColumnImpl* clone() const override;
    size_t n_children() const noexcept override;
    const Column& child(size_t i) const override;
    size_t num_buffers() const noexcept override;
    const void* get_buffer(size_t i) const override;

    bool get_element(size_t i, Column* out) const override;
};


extern template class ArrowArray_ColumnImpl<uint32_t>;
extern template class ArrowArray_ColumnImpl<uint64_t>;




}  // namespace dt
#endif
