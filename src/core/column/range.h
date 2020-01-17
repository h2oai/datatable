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
  * Virtual column corresponding to Python `range()` object. This
  * column is created when a `range` object is passed into Frame
  * constructor.
  *
  * By default, this column will take stype INT32. However, if the
  * range is sufficiently large, the stype will become INT64. However,
  * we do not support further promotion into FLOAT64 stype for even
  * larger integers.
  */
class Range_ColumnImpl : public Virtual_ColumnImpl {
  private:
    int64_t start_;
    int64_t step_;

  public:
    Range_ColumnImpl(int64_t start, int64_t stop, int64_t step,
                     SType stype = SType::VOID);

    ColumnImpl* clone() const override;
    size_t memory_footprint() const noexcept override;
    void materialize(Column&, bool) override;
    void verify_integrity() const override;

    bool get_element(size_t, int8_t*)  const override;
    bool get_element(size_t, int16_t*) const override;
    bool get_element(size_t, int32_t*) const override;
    bool get_element(size_t, int64_t*) const override;
    bool get_element(size_t, float*)   const override;
    bool get_element(size_t, double*)  const override;

    void fill_npmask(bool* outmask, size_t row0, size_t row1) const override;
    void apply_rowindex(const RowIndex& ri, Column& out) override;

  private:
    Range_ColumnImpl(size_t, SType, int64_t, int64_t);  // for cloning

    // Helper for get_element() accessors
    template <typename T> inline bool _get(size_t, T*) const;

    // Helper for materialize()
    template <typename T> void _materialize(Column&) const;
};




}  // namespace dt
#endif
