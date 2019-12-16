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
#include "column.h"
template<> inline py::robj GETNA() { return py::rnone(); }
namespace dt {


//------------------------------------------------------------------------------
// SentinelFw_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
SentinelFw_ColumnImpl<T>::SentinelFw_ColumnImpl(ColumnImpl*&& other)
  : Sentinel_ColumnImpl(other->nrows(), other->stype())
{
  assert_compatible_type<T>(other->stype());
  auto fwother = dynamic_cast<SentinelFw_ColumnImpl<T>*>(other);
  xassert(fwother != nullptr);
  mbuf_ = std::move(fwother->mbuf_);
  stats_ = std::move(fwother->stats_);
  delete other;
}

SentinelBool_ColumnImpl::SentinelBool_ColumnImpl(ColumnImpl*&& other)
  : SentinelFw_ColumnImpl<int8_t>(std::move(other)) {}



template <typename T>
SentinelFw_ColumnImpl<T>::SentinelFw_ColumnImpl(size_t nrows)
  : Sentinel_ColumnImpl(nrows, stype_from<T>())
{
  mbuf_.resize(sizeof(T) * nrows);
}

SentinelBool_ColumnImpl::SentinelBool_ColumnImpl(size_t nrows)
  : SentinelFw_ColumnImpl<int8_t>(nrows)
{
  stype_ = SType::BOOL;
}

SentinelObj_ColumnImpl::SentinelObj_ColumnImpl(size_t nrows_)
  : SentinelFw_ColumnImpl<py::robj>(nrows_)
{
  stype_ = SType::OBJ;
  mbuf_.set_pyobjects(/*clear_data = */ true);
}


template <typename T>
SentinelFw_ColumnImpl<T>::SentinelFw_ColumnImpl(size_t nrows, Buffer&& mr)
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

SentinelBool_ColumnImpl::SentinelBool_ColumnImpl(size_t nrows, Buffer&& mem)
  : SentinelFw_ColumnImpl<int8_t>(nrows, std::move(mem))
{
  stype_ = SType::BOOL;
}

SentinelObj_ColumnImpl::SentinelObj_ColumnImpl(size_t nrows_, Buffer&& mem)
    : SentinelFw_ColumnImpl<py::robj>(nrows_, std::move(mem))
{
  stype_ = SType::OBJ;
  mbuf_.set_pyobjects(/*clear_data = */ false);
}




template <typename T>
ColumnImpl* SentinelFw_ColumnImpl<T>::clone() const {
  return new SentinelFw_ColumnImpl<T>(nrows_, Buffer(mbuf_));
}

ColumnImpl* SentinelBool_ColumnImpl::clone() const {
  return new SentinelBool_ColumnImpl(nrows_, Buffer(mbuf_));
}

ColumnImpl* SentinelObj_ColumnImpl::clone() const {
  return new SentinelObj_ColumnImpl(nrows_, Buffer(mbuf_));
}



template <typename T>
void SentinelFw_ColumnImpl<T>::materialize(Column&, bool to_memory) {
  if (to_memory) {
    mbuf_.to_memory();
  }
}




template <typename T>
size_t SentinelFw_ColumnImpl<T>::memory_footprint() const noexcept {
  return sizeof(*this) + (stats_? stats_->memory_footprint() : 0)
                       + mbuf_.memory_footprint();
}




//------------------------------------------------------------------------------
// Data access
//------------------------------------------------------------------------------

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, int8_t* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<int8_t>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, int16_t* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<int16_t>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, int32_t* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<int32_t>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, int64_t* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<int64_t>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, float* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<float>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, double* out) const {
  T x = static_cast<const T*>(mbuf_.rptr())[i];
  *out = static_cast<double>(x);
  return !ISNA<T>(x);
}


template <typename T>
bool SentinelFw_ColumnImpl<T>::get_element(size_t i, py::robj* out) const {
  return ColumnImpl::get_element(i, out);
}

bool SentinelObj_ColumnImpl::get_element(size_t i, py::robj* out) const {
  py::robj x = static_cast<const py::robj*>(mbuf_.rptr())[i];
  *out = x;
  return !x.is_none();
}





//------------------------------------------------------------------------------
// Data buffers
//------------------------------------------------------------------------------

template <typename T>
size_t SentinelFw_ColumnImpl<T>::get_num_data_buffers() const noexcept {
  return 1;
}


template <typename T>
bool SentinelFw_ColumnImpl<T>::is_data_editable(size_t k) const {
  xassert(k == 0); (void) k;
  return mbuf_.is_writable();
}


template <typename T>
size_t SentinelFw_ColumnImpl<T>::get_data_size(size_t k) const {
  xassert(k == 0); (void) k;
  xassert(mbuf_.size() >= nrows_ * sizeof(T));
  return nrows_ * sizeof(T);
}


template <typename T>
const void* SentinelFw_ColumnImpl<T>::get_data_readonly(size_t k) const {
  xassert(k == 0); (void) k;
  return mbuf_.rptr();
}


template <typename T>
void* SentinelFw_ColumnImpl<T>::get_data_editable(size_t k) {
  xassert(k == 0); (void) k;
  return mbuf_.wptr();
}


template <typename T>
Buffer SentinelFw_ColumnImpl<T>::get_data_buffer(size_t k) const {
  xassert(k == 0); (void) k;
  return Buffer(mbuf_);
}




//------------------------------------------------------------------------------
// Column operations
//------------------------------------------------------------------------------

template <typename T>
void SentinelFw_ColumnImpl<T>::replace_values(const RowIndex& replace_at, T replace_with) {
  T* data = static_cast<T*>(mbuf_.wptr());
  replace_at.iterate(0, replace_at.size(), 1,
    [&](size_t, size_t j, bool jvalid) {
      if (!jvalid) return;
      data[j] = replace_with;
    });
  if (stats_) stats_->reset();
}


template <typename T>
void SentinelFw_ColumnImpl<T>::replace_values(
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
    [&](size_t i, size_t j, bool jvalid) {
      if (!jvalid) return;
      T value;
      bool isvalid = replace_with.get_element(i, &value);
      data_dest[j] = isvalid? value : GETNA<T>();
    });
}




// Explicit instantiations
template class SentinelFw_ColumnImpl<int8_t>;
template class SentinelFw_ColumnImpl<int16_t>;
template class SentinelFw_ColumnImpl<int32_t>;
template class SentinelFw_ColumnImpl<int64_t>;
template class SentinelFw_ColumnImpl<float>;
template class SentinelFw_ColumnImpl<double>;
template class SentinelFw_ColumnImpl<py::robj>;


} // namespace dt
