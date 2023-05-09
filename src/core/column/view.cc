//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
  : Virtual_ColumnImpl(ri.size(), col.data_type()),
    arg_(std::move(col)),
    start_(ri.slice_start()),
    step_(ri.slice_step())
{
  xassert(ri.isslice());
  xassert(ri.max() < arg_.nrows());
}

SliceView_ColumnImpl::SliceView_ColumnImpl(
    Column&& col, size_t start, size_t count, size_t step)
  : Virtual_ColumnImpl(count, col.data_type()),
    arg_(std::move(col)),
    start_(start),
    step_(step)
{
  xassert((start < arg_.nrows()) || (count == 0 && start == arg_.nrows()));
  xassert(start + count*step <= arg_.nrows());
}


ColumnImpl* SliceView_ColumnImpl::clone() const {
  return new SliceView_ColumnImpl(
                Column(arg_), RowIndex(start_, nrows_, step_));
}

bool SliceView_ColumnImpl::allow_parallel_access() const {
  return arg_.allow_parallel_access();
}

size_t SliceView_ColumnImpl::n_children() const noexcept {
  return 1;
}

const Column& SliceView_ColumnImpl::child(size_t i) const {
  xassert(i == 0);  (void)i;
  return arg_;
}


bool SliceView_ColumnImpl::get_element(size_t i, int8_t* out)   const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int16_t* out)  const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int32_t* out)  const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, int64_t* out)  const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, float* out)    const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, double* out)   const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, CString* out)  const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, py::oobj* out) const { return arg_.get_element(start_ + i*step_, out); }
bool SliceView_ColumnImpl::get_element(size_t i, Column* out)   const { return arg_.get_element(start_ + i*step_, out); }




//------------------------------------------------------------------------------
// ArrayView_ColumnImpl
//------------------------------------------------------------------------------
static_assert(RowIndex::NA<int32_t> < 0, "Unexpected RowIndex::NA<int32_t>");
static_assert(RowIndex::NA<int64_t> < 0, "Unexpected RowIndex::NA<int64_t>");

template <typename T> const T* get_indices(const RowIndex&) { return nullptr; }
template <> const int32_t* get_indices(const RowIndex& ri) { return ri.indices32(); }
template <> const int64_t* get_indices(const RowIndex& ri) { return ri.indices64(); }


template <typename T>
ArrayView_ColumnImpl<T>::ArrayView_ColumnImpl(
    Column&& col, const RowIndex& ri, size_t nrows)
  : Virtual_ColumnImpl(nrows, col.data_type()),
    arg(std::move(col))
{
  set_rowindex(ri);
}

template <typename T>
void ArrayView_ColumnImpl<T>::set_rowindex(const RowIndex& ri) {
  xassert((std::is_same<T, int32_t>::value? ri.isarr32() : ri.isarr64()));
  xassert(ri.max() < arg.nrows());
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
size_t ArrayView_ColumnImpl<T>::n_children() const noexcept {
  return 1;
}

template <typename T>
const Column& ArrayView_ColumnImpl<T>::child(size_t i) const {
  xassert(i == 0);  (void)i;
  return arg;
}


template <typename T>
template <typename S>
bool ArrayView_ColumnImpl<T>::_get_element(size_t i, S* out) const {
  xassert(i < nrows_);
  T j = indices[i];
  if (j < 0) return false;
  return arg.get_element(static_cast<size_t>(j), out);
}

template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, int8_t* out)   const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, int16_t* out)  const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, int32_t* out)  const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, int64_t* out)  const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, float* out)    const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, double* out)   const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, CString* out)  const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, py::oobj* out) const { return _get_element(i, out); }
template <typename T> bool ArrayView_ColumnImpl<T>::get_element(size_t i, Column* out)   const { return _get_element(i, out); }


template class ArrayView_ColumnImpl<int32_t>;
template class ArrayView_ColumnImpl<int64_t>;




//------------------------------------------------------------------------------
// base ColumnImpl
//------------------------------------------------------------------------------

// factory function
static Column _make_view(Column&& col, const RowIndex& ri) {
  // This covers the case when ri.size()==0, and when all elements are NAs
  if (ri.is_all_missing()) {
    return Column::new_na_column(ri.size(), col.data_type());
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
