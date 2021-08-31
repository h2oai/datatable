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
#ifndef dt_COLUMN_CATEGORICAL_h
#define dt_COLUMN_CATEGORICAL_h

#include "column/virtual.h"


namespace dt {

template <typename T>
class Categorical_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Buffer codes_;
    Column categories_;

  public:
    Categorical_ColumnImpl(size_t nrows, Buffer&& codes, Column&& categories);

    ColumnImpl* clone() const override;
    size_t n_children() const noexcept override;
    const Column& child(size_t i) const override;
    size_t num_buffers() const noexcept;
    Buffer get_buffer() const noexcept;

    template <class U>
    bool get_element_(size_t, U*) const;

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::oobj*) const override;
    bool get_element(size_t, Column*)   const override;

};


extern template class Categorical_ColumnImpl<uint8_t>;
extern template class Categorical_ColumnImpl<uint16_t>;
extern template class Categorical_ColumnImpl<uint32_t>;


}  // namespace dt
#endif
