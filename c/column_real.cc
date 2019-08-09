//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "csv/toa.h"
#include "datatablemodule.h"
#include "python/float.h"

template <typename T> constexpr SType stype_for() { return SType::VOID; }
template <> constexpr SType stype_for<float>()  { return SType::FLOAT32; }
template <> constexpr SType stype_for<double>() { return SType::FLOAT64; }

template <typename T>
RealColumn<T>::RealColumn(size_t nrows) : FwColumn<T>(nrows) {
  this->_stype = stype_for<T>();
}

template <typename T>
RealColumn<T>::RealColumn(size_t nrows, MemoryRange&& mem)
  : FwColumn<T>(nrows, std::move(mem))
{
  this->_stype = stype_for<T>();
}


template <typename T>
bool RealColumn<T>::get_element(size_t i, T* out) const {
  size_t j = (this->ri)[i];
  if (j == RowIndex::NA) return true;
  T x = this->elements_r()[j];
  *out = x;
  return ISNA<T>(x);
}



//------------------------------------------------------------------------------


// Explicit instantiation of the template
template class RealColumn<float>;
template class RealColumn<double>;
