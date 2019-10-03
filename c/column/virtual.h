//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_COLUMN_VIRTUAL_h
#define dt_COLUMN_VIRTUAL_h
#include <memory>
#include "column/column_impl.h"
namespace dt {


/**
  * Base class for all virtual columns.
  */
class Virtual_ColumnImpl : public ColumnImpl
{
  public:
    Virtual_ColumnImpl(size_t nrows, SType stype);

    bool is_virtual() const noexcept override;
    size_t memory_footprint() const noexcept override;

    NaStorage get_na_storage_method() const noexcept override;
    size_t get_num_data_buffers() const noexcept override;
    bool is_data_editable(size_t) const override;
    size_t get_data_size(size_t) const override;
    const void* get_data_readonly(size_t k) const override;
    void* get_data_editable(size_t k) override;
    Buffer get_data_buffer(size_t k) const override;

};



}  // namespace dt
#endif
