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
#ifndef dt_COLUMN_RANGE_h
#define dt_COLUMN_RANGE_h
#include <memory>
#include "column/virtual.h"
namespace dt {


/**
  * Virtual column representing the `arg` column repeated n times.
  */
class Range_ColumnImpl : public Virtual_ColumnImpl {
  private:
    int64_t start_;
    int64_t step_;

  public:
    Range_ColumnImpl(int64_t start, int64_t stop, int64_t step,
                     SType stype = SType::VOID);

    ColumnImpl* shallowcopy() const override;

    bool get_element(size_t, int8_t*)  const override;
    bool get_element(size_t, int16_t*) const override;
    bool get_element(size_t, int32_t*) const override;
    bool get_element(size_t, int64_t*) const override;
    bool get_element(size_t, float*)   const override;
    bool get_element(size_t, double*)  const override;

  private:
    Range_ColumnImpl(SType, size_t, int64_t, int64_t);  // for cloning
};




}  // namespace dt
#endif
