//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/int.h"
#include "column_impl.h"
#include "datatablemodule.h"

template <typename T> constexpr SType stype_for() { return SType::VOID; }
template <> constexpr SType stype_for<int8_t>()  { return SType::INT8; }
template <> constexpr SType stype_for<int16_t>() { return SType::INT16; }
template <> constexpr SType stype_for<int32_t>() { return SType::INT32; }
template <> constexpr SType stype_for<int64_t>() { return SType::INT64; }


template <typename T>
IntColumn<T>::IntColumn(size_t nrows) : FwColumn<T>(nrows) {
  this->_stype = stype_for<T>();
}

template <typename T>
IntColumn<T>::IntColumn(size_t nrows, MemoryRange&& mem)
  : FwColumn<T>(nrows, std::move(mem))
{
  this->_stype = stype_for<T>();
}


template <typename T>
bool IntColumn<T>::get_element(size_t i, int32_t* out) const {
  size_t j = (this->ri)[i];
  if (j == RowIndex::NA) return true;
  T x = this->elements_r()[j];
  *out = static_cast<int32_t>(x);
  return ISNA<T>(x);
}

template <typename T>
bool IntColumn<T>::get_element(size_t i, int64_t* out) const {
  size_t j = (this->ri)[i];
  if (j == RowIndex::NA) return true;
  T x = this->elements_r()[j];
  *out = static_cast<int64_t>(x);
  return ISNA<T>(x);
}




//------------------------------------------------------------------------------

// Explicit instantiation of the template
template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
