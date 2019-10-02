//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <type_traits>
#include "column/sentinel_fw.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "parallel/api.h"  // dt::parallel_for_static
#include "column.h"
template<> inline py::robj GETNA() { return py::rnone(); }
namespace dt {


//------------------------------------------------------------------------------
// FwColumn
//------------------------------------------------------------------------------

template <typename T>
FwColumn<T>::FwColumn(ColumnImpl*&& other)
  : Sentinel_ColumnImpl(other->nrows(), other->stype())
{
  assert_compatible_type<T>(other->stype());
  auto fwother = dynamic_cast<FwColumn<T>*>(other);
  xassert(fwother != nullptr);
  mbuf_ = std::move(fwother->mbuf_);
  stats_ = std::move(fwother->stats_);
}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows)
  : Sentinel_ColumnImpl(nrows, stype_from<T>())
{
  mbuf_.resize(sizeof(T) * nrows);
}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows, Buffer&& mr)
  : Sentinel_ColumnImpl(nrows, stype_from<T>())
{
  size_t req_size = sizeof(T) * nrows;
  if (mr) {
    xassert(mr.size() >= req_size);
  } else {
    mr.resize(req_size);
  }
  mbuf_ = std::move(mr);
}


template <typename T>
ColumnImpl* FwColumn<T>::clone() const {
  return new FwColumn<T>(nrows_, Buffer(mbuf_));
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
bool FwColumn<T>::get_element(size_t i, T* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = x;
  return !ISNA<T>(x);
}





//------------------------------------------------------------------------------
// Data buffers
//------------------------------------------------------------------------------

template <typename T>
size_t FwColumn<T>::get_num_data_buffers() const noexcept {
  return 1;
}


template <typename T>
bool FwColumn<T>::is_data_editable(size_t k) const {
  xassert(k == 0);
  return mbuf_.is_writable();
}


template <typename T>
size_t FwColumn<T>::get_data_size(size_t k) const {
  xassert(k == 0);
  xassert(mbuf_.size() >= nrows_ * sizeof(T));
  return nrows_ * sizeof(T);
}


template <typename T>
const void* FwColumn<T>::get_data_readonly(size_t k) const {
  xassert(k == 0);
  return mbuf_.rptr();
}


template <typename T>
void* FwColumn<T>::get_data_editable(size_t k) {
  xassert(k == 0);
  return mbuf_.wptr();
}


template <typename T>
Buffer FwColumn<T>::get_data_buffer(size_t k) const {
  xassert(k == 0);
  return Buffer(mbuf_);
}




//------------------------------------------------------------------------------
// Column operations
//------------------------------------------------------------------------------

template <typename T>
void FwColumn<T>::replace_values(const RowIndex& replace_at, T replace_with) {
  T* data = static_cast<T*>(mbuf_.wptr());
  replace_at.iterate(0, replace_at.size(), 1,
    [&](size_t, size_t j) {
      data[j] = replace_with;
    });
  if (stats_) stats_->reset();
}


template <typename T>
void FwColumn<T>::replace_values(
    const RowIndex& replace_at, const Column& replace_with, Column&)
{
  if (!replace_with) {
    return replace_values(replace_at, GETNA<T>());
  }
  Column with = (replace_with.stype() == stype_)
                    ? replace_with  // copy
                    : replace_with.cast(stype_);

  if (with.nrows() == 1) {
    T replace_value;
    bool isvalid = with.get_element(0, &replace_value);
    return isvalid? replace_values(replace_at, replace_value) :
                    replace_values(replace_at, GETNA<T>());
  }

  size_t replace_n = replace_at.size();
  T* data_dest = static_cast<T*>(mbuf_.wptr());
  xassert(with.nrows() == replace_n);

  replace_at.iterate(0, replace_n, 1,
    [&](size_t i, size_t j) {
      if (j == RowIndex::NA) return;
      T value;
      bool isvalid = replace_with.get_element(i, &value);
      data_dest[j] = isvalid? value : GETNA<T>();
    });
}


template <typename T>
size_t FwColumn<T>::memory_footprint() const noexcept {
  return sizeof(*this) + (stats_? stats_->memory_footprint() : 0)
                       + mbuf_.memory_footprint();
}

template <typename T>
void FwColumn<T>::verify_integrity() const {
  Sentinel_ColumnImpl::verify_integrity();
  mbuf_.verify_integrity();
}



// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<py::robj>;


} // namespace dt
