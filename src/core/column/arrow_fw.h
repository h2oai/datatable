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
#ifndef dt_COLUMN_ARROW_FW_h
#define dt_COLUMN_ARROW_FW_h
#include "column/arrow.h"
namespace dt {


/**
  * TODO: make this class material instead of virtual
  */
class ArrowFw_ColumnImpl : public Arrow_ColumnImpl {
  private:
    Buffer validity_;
    Buffer data_;

  public:
    ArrowFw_ColumnImpl(size_t nrows, SType stype,
                       Buffer&& valid, Buffer&& data);

    ColumnImpl* clone() const override;
    // void materialize(Column&, bool) override;

    bool get_element(size_t, int8_t*)  const override;
    bool get_element(size_t, int16_t*) const override;
    bool get_element(size_t, int32_t*) const override;
    bool get_element(size_t, int64_t*) const override;
    bool get_element(size_t, float*)   const override;
    bool get_element(size_t, double*)  const override;

    size_t num_buffers() const noexcept override;
    const void* get_buffer(size_t i) const override;

  private:
    template <typename T> inline bool _get(size_t, T*) const;
};




}  // namespace dt
#endif
