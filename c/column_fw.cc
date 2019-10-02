//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <type_traits>
#include "column/sentinel_fw.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "parallel/api.h"  // dt::parallel_for_static
#include "column.h"
template<> inline py::robj GETNA() { return py::rnone(); }

namespace dt {


template <typename T> constexpr SType stype_for(){ return SType::VOID; }
template <> constexpr SType stype_for<int8_t>()  { return SType::INT8; }
template <> constexpr SType stype_for<int16_t>() { return SType::INT16; }
template <> constexpr SType stype_for<int32_t>() { return SType::INT32; }
template <> constexpr SType stype_for<int64_t>() { return SType::INT64; }
template <> constexpr SType stype_for<float>()   { return SType::FLOAT32; }
template <> constexpr SType stype_for<double>()  { return SType::FLOAT64; }



/**
 * Private constructor that creates an "invalid" column. An `init_X` method
 * should be subsequently called before using this column.
 */
template <typename T>
FwColumn<T>::FwColumn()
  : Sentinel_ColumnImpl(0, stype_for<T>()) {}


template <typename T>
FwColumn<T>::FwColumn(ColumnImpl*&& other)
  : Sentinel_ColumnImpl(other->nrows(), other->stype())
{
  auto fwother = dynamic_cast<FwColumn<T>*>(other);
  xassert(fwother != nullptr);
  mbuf = std::move(fwother->mbuf);
  stats_ = std::move(fwother->stats_);
}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows_)
  : Sentinel_ColumnImpl(nrows_, stype_for<T>())
{
  mbuf.resize(sizeof(T) * nrows_);
}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows_, Buffer&& mr)
  : Sentinel_ColumnImpl(nrows_, stype_for<T>())
{
  size_t req_size = sizeof(T) * nrows_;
  if (mr) {
    xassert(mr.size() >= req_size);
  } else {
    mr.resize(req_size);
  }
  mbuf = std::move(mr);
}


/**
 * Create a shallow copy of the column; possibly applying the provided rowindex.
 */
template <typename T>
ColumnImpl* FwColumn<T>::shallowcopy() const {
  return new FwColumn<T>(nrows_, Buffer(mbuf));
}



//==============================================================================
// Data access
//==============================================================================


template <typename T>
const T* FwColumn<T>::elements_r() const {
  return static_cast<const T*>(mbuf.rptr());
}

template <typename T>
T* FwColumn<T>::elements_w() {
  return static_cast<T*>(mbuf.wptr());
}


template <typename T>
T FwColumn<T>::get_elem(size_t i) const {
  return static_cast<const T*>(mbuf.rptr())[i];
}

template <typename T>
bool FwColumn<T>::get_element(size_t i, T* out) const {
  T x = static_cast<const T*>(mbuf.rptr())[i];
  *out = x;
  return !ISNA<T>(x);
}


template <typename T>
size_t FwColumn<T>::get_num_data_buffers() const noexcept {
  return 1;
}


template <typename T>
bool FwColumn<T>::is_data_editable(size_t k) const {
  xassert(k == 0);
  return mbuf.is_writable();
}


template <typename T>
size_t FwColumn<T>::get_data_size(size_t k) const {
  xassert(k == 0);
  xassert(mbuf.size() >= nrows_ * sizeof(T));
  return nrows_ * sizeof(T);
}


template <typename T>
const void* FwColumn<T>::get_data_readonly(size_t k) const {
  xassert(k == 0);
  return mbuf.rptr();
}


template <typename T>
void* FwColumn<T>::get_data_editable(size_t k) {
  xassert(k == 0);
  return mbuf.wptr();
}


template <typename T>
Buffer FwColumn<T>::get_data_buffer(size_t k) const {
  xassert(k == 0);
  return Buffer(mbuf);
}





template <typename T>
void FwColumn<T>::replace_values(const RowIndex& replace_at, T replace_with) {
  T* data = elements_w();  // This will materialize the column if necessary
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
  T* data_dest = elements_w();
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
                       + mbuf.memory_footprint();
}

template <typename T>
void FwColumn<T>::verify_integrity() const {
  Sentinel_ColumnImpl::verify_integrity();
  mbuf.verify_integrity();
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
