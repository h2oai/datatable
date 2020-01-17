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
#ifndef dt_COLUMN_SENTINEL_h
#define dt_COLUMN_SENTINEL_h
#include <memory>
#include "column/column_impl.h"
namespace dt {


/**
  * Base class for all material columns that use sentinel values
  * in order to encode NAs.
  */
class Sentinel_ColumnImpl : public ColumnImpl
{
  public:
    static Column make_column(size_t nrows, SType);
    static Column make_fw_column(size_t nrows, SType, Buffer&&);
    static Column make_str_column(size_t nrows, Buffer&&, Buffer&&);

    bool is_virtual() const noexcept override;
    NaStorage get_na_storage_method() const noexcept override;

  protected:
    Sentinel_ColumnImpl(size_t nrows, SType stype);
};



}  // namespace dt
#endif
