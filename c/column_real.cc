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



template <typename T>
SType RealColumn<T>::stype() const noexcept {
  return sizeof(T) == 4? SType::FLOAT32 :
         sizeof(T) == 8? SType::FLOAT64 : SType::VOID;
}

template <typename T>
py::oobj RealColumn<T>::get_value_at_index(size_t i) const {
  size_t j = (this->ri)[i];
  T x = this->elements_r()[j];
  return ISNA<T>(x)? py::None() : py::ofloat(x);
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

template <typename T>
RealStats<T>* RealColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new RealStats<T>();
  return static_cast<RealStats<T>*>(stats);
}

template <typename T> T      RealColumn<T>::min() const  { return get_stats()->min(this); }
template <typename T> T      RealColumn<T>::max() const  { return get_stats()->max(this); }
template <typename T> T      RealColumn<T>::mode() const { return get_stats()->mode(this); }
template <typename T> double RealColumn<T>::sum() const  { return get_stats()->sum(this); }
template <typename T> double RealColumn<T>::mean() const { return get_stats()->mean(this); }
template <typename T> double RealColumn<T>::sd() const   { return get_stats()->stdev(this); }
template <typename T> double RealColumn<T>::skew() const { return get_stats()->skew(this); }
template <typename T> double RealColumn<T>::kurt() const { return get_stats()->kurt(this); }




//------------------------------------------------------------------------------


// Explicit instantiation of the template
template class RealColumn<float>;
template class RealColumn<double>;
