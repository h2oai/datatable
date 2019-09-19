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
    SliceView_ColumnImpl(Column&& col, const RowIndex& ri)
      : Virtual_ColumnImpl(ri.size(), col.stype()),
        arg(std::move(col)),
        start(ri.slice_start()),
        step(ri.slice_step())
    {
      xassert(ri.isslice());
      xassert(ri.max() < arg.nrows());
    }

    ColumnImpl* shallowcopy() const override {
      return new SliceView_ColumnImpl(Column(arg), RowIndex(start, _nrows, step));
    }

    bool get_element(size_t i, int8_t* out)   const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, int16_t* out)  const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, int32_t* out)  const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, int64_t* out)  const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, float* out)    const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, double* out)   const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, CString* out)  const override { return arg.get_element(start + i*step, out); }
    bool get_element(size_t i, py::robj* out) const override { return arg.get_element(start + i*step, out); }
};





//------------------------------------------------------------------------------
// ArrayView_ColumnImpl
//------------------------------------------------------------------------------

template <typename T> const T* get_indices(const RowIndex&) { return nullptr; }
template <> const int32_t* get_indices(const RowIndex& ri) { return ri.indices32(); }
template <> const int64_t* get_indices(const RowIndex& ri) { return ri.indices64(); }


template <typename T>
class ArrayView_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    RowIndex rowindex; // owns the `indices` array
    const T* indices;

  public:
    ArrayView_ColumnImpl(Column&& col, const RowIndex& ri)
      : Virtual_ColumnImpl(ri.size(), col.stype()),
        arg(std::move(col)),
        rowindex(ri),
        indices(get_indices<T>(ri))
    {
      xassert((std::is_same<T, int32_t>::value? ri.isarr32() : ri.isarr64()));
      xassert(ri.max() < arg.nrows());
    }

    ColumnImpl* shallowcopy() const override {
      return new ArrayView_ColumnImpl<T>(Column(arg), rowindex);
    }


    bool get_element(size_t i, int8_t* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, int16_t* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, int32_t* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, int64_t* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, float* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, double* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, CString* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }

    bool get_element(size_t i, py::robj* out) const override {
      T j = indices[i];
      if (j < 0) return true;
      return arg.get_element(static_cast<size_t>(j), out);
    }
};





}  // namespace dt

//------------------------------------------------------------------------------
// base ColumnImpl
//------------------------------------------------------------------------------

// factory function
static Column _make_view(Column&& col, const RowIndex& ri) {
  if (ri.size() == 0) {
    return Column::new_na_column(col.stype(), 0);
  }
  switch (ri.type()) {
    case RowIndexType::SLICE:
      return Column(new dt::SliceView_ColumnImpl(std::move(col), ri));

    case RowIndexType::ARR32:
      return Column(new dt::ArrayView_ColumnImpl<int32_t>(std::move(col), ri));

    case RowIndexType::ARR64:
      return Column(new dt::ArrayView_ColumnImpl<int64_t>(std::move(col), ri));

    default:
      throw RuntimeError()
        << "Invalid Rowindex type: " << static_cast<int>(ri.type());
  }
}


void ColumnImpl::apply_rowindex(const RowIndex& rowindex, Column& out) {
  if (!rowindex) return;
  out = _make_view(std::move(out), rowindex);
}
