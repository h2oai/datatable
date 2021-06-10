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
#ifndef dt_COLUMN_TIME_SCALED_h
#define dt_COLUMN_TIME_SCALED_h
#include "_dt.h"
#include "column/virtual.h"
namespace dt {


/**
  * Time64 column which is produced from the underlying column by
  * multiplying all values by a constant `scale_` factor.
  *
  * This can be used for converting time columns that store their
  * values at a resolution different from nanosecond. For example,
  * numpy may have a column of type `datetime64[s]`, or even when
  * we need to convert date32 column (resolution of days) into time64
  * (resolution of ns).
  */
class TimeScaled_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;
    int64_t scale_;

  public:
    TimeScaled_ColumnImpl(Column&& arg, int64_t scale);

    ColumnImpl* clone() const override;
    size_t n_children() const noexcept override;
    const Column& child(size_t i) const override;

    bool get_element(size_t i, int64_t* out) const override;
};



}  // namespace dt
#endif
