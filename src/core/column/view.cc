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
#include "column/view.h"
namespace dt {



//------------------------------------------------------------------------------
// SliceView_ColumnImpl
//------------------------------------------------------------------------------

SliceView_ColumnImpl::SliceView_ColumnImpl(
    Column&& col, const RowIndex& ri)
  : Virtual_ColumnImpl(ri.size(), col.stype()),
    arg(std::move(col)),
    start(ri.slice_start()),
    step(ri.slice_step())
{
  xassert(ri.isslice());
  xassert(ri.max() < arg.nrows());
}


ColumnImpl* SliceView_ColumnImpl::clone() const {
  return new SliceView_ColumnImpl(
                Column(arg), RowIndex(start, nrows_, step));
}

bool SliceView_ColumnImpl::allow_parallel_access() const {
  return arg.allow_parallel_access();
}


bool SliceView_ColumnImpl::get_element(size_t i, int8_t* out)   const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int16_t* out)  const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int32_t* out)  const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int64_t* out)  const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, float* out)    const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, double* out)   const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, CString* out)  const { return arg.get_element(start + i*step, out); }
bool SliceView_ColumnImpl::get_element(size_t i, py::robj* out) const { return arg.get_element(start + i*step, out); }




//------------------------------------------------------------------------------
// ArrayView_ColumnImpl
//------------------------------------------------------------------------------
static_assert(RowIndex::NA_ARR32 < 0, "Unexpected RowIndex::NA_ARR32");
static_assert(RowIndex::NA_ARR64 < 0, "Unexpected RowIndex::NA_ARR64");

template <typename T> const T* get_indices(const RowIndex&) { return nullptr; }
template <> const int32_t* get_indices(const RowIndex& ri) { return ri.indices32(); }
template <> const int64_t* get_indices(const RowIndex& ri) { return ri.indices64(); }


template <typename T>
ArrayView_ColumnImpl<T>::ArrayView_ColumnImpl(
    Column&& col, const RowIndex& ri, size_t nrows)
  : Virtual_ColumnImpl(nrows, col.stype()),
    arg(std::move(col))
{
  xassert((std::is_same<T, int32_t>::value? ri.isarr32() : ri.isarr64()));
  xassert(ri.max() < arg.nrows());
  set_rowindex(ri);
}

template <typename T>
void ArrayView_ColumnImpl<T>::set_rowindex(const RowIndex& ri) {
  rowindex_container = ri;
  indices = get_indices<T>(ri);
}


template <typename T>
ColumnImpl* ArrayView_ColumnImpl<T>::clone() const {
  return new ArrayView_ColumnImpl<T>(Column(arg), rowindex_container, nrows_);
}


template <typename T>
bool ArrayView_ColumnImpl<T>::allow_parallel_access() const {
  return arg.allow_parallel_access();
}



template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, int8_t* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, int16_t* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, int32_t* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, int64_t* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, float* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, double* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T>
bool ArrayView_ColumnImpl<T>::get_element(size_t i, py::robj* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}


template class ArrayView_ColumnImpl<int32_t>;
template class ArrayView_ColumnImpl<int64_t>;




//------------------------------------------------------------------------------
// base ColumnImpl
//------------------------------------------------------------------------------

// factory function
static Column _make_view(Column&& col, const RowIndex& ri) {
  // This covers the case when ri.size()==0, and when all elements are NAs
  if (ri.is_all_missing()) {
    return Column::new_na_column(ri.size(), col.stype());
  }
  switch (ri.type()) {
    case RowIndexType::SLICE:
      return Column(new dt::SliceView_ColumnImpl(std::move(col), ri));

    case RowIndexType::ARR32:
      return Column(new dt::ArrayView_ColumnImpl<int32_t>(
                      std::move(col), ri, ri.size()));

    case RowIndexType::ARR64:
      return Column(new dt::ArrayView_ColumnImpl<int64_t>(
                      std::move(col), ri, ri.size()));

    default:
      throw RuntimeError()
        << "Invalid Rowindex type: " << static_cast<int>(ri.type());
  }
}


void ColumnImpl::apply_rowindex(const RowIndex& rowindex, Column& out) {
  if (!rowindex) return;
  out = _make_view(std::move(out), rowindex);
}



}  // namespace dt
