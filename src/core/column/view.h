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
#ifndef dt_COLUMN_VIEW_h
#define dt_COLUMN_VIEW_h
#include "column/virtual.h"
namespace dt {



//------------------------------------------------------------------------------
// SliceView_ColumnImpl
//------------------------------------------------------------------------------

class SliceView_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    size_t start;
    size_t step;

  public:
    SliceView_ColumnImpl(Column&& col, const RowIndex& ri);
    ColumnImpl* clone() const override;
    bool allow_parallel_access() const override;

    bool get_element(size_t i, int8_t* out)   const override;
    bool get_element(size_t i, int16_t* out)  const override;
    bool get_element(size_t i, int32_t* out)  const override;
    bool get_element(size_t i, int64_t* out)  const override;
    bool get_element(size_t i, float* out)    const override;
    bool get_element(size_t i, double* out)   const override;
    bool get_element(size_t i, CString* out)  const override;
    bool get_element(size_t i, py::robj* out) const override;
};





//------------------------------------------------------------------------------
// ArrayView_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
class ArrayView_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    RowIndex rowindex_container; // owns the `indices` array
    const T* indices;

  public:
    ArrayView_ColumnImpl(Column&& col, const RowIndex& ri, size_t nrows);
    ColumnImpl* clone() const override;
    bool allow_parallel_access() const override;

    // defined in sort.cc
    void sort_grouped(const Groupby& gby, Column& out) override;

    bool get_element(size_t i, int8_t* out)   const override;
    bool get_element(size_t i, int16_t* out)  const override;
    bool get_element(size_t i, int32_t* out)  const override;
    bool get_element(size_t i, int64_t* out)  const override;
    bool get_element(size_t i, float* out)    const override;
    bool get_element(size_t i, double* out)   const override;
    bool get_element(size_t i, CString* out)  const override;
    bool get_element(size_t i, py::robj* out) const override;

  private:
    void set_rowindex(const RowIndex&);
};


extern template class ArrayView_ColumnImpl<int32_t>;
extern template class ArrayView_ColumnImpl<int64_t>;




}  // namespace dt
#endif
