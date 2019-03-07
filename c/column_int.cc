//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "datatablemodule.h"
#include "python/int.h"
#include "py_utils.h"
#include "utils/parallel.h"



template <typename T>
SType IntColumn<T>::stype() const noexcept {
  return sizeof(T) == 1? SType::INT8 :
         sizeof(T) == 2? SType::INT16 :
         sizeof(T) == 4? SType::INT32 :
         sizeof(T) == 8? SType::INT64 : SType::VOID;
}

template <typename T>
py::oobj IntColumn<T>::get_value_at_index(size_t i) const {
  size_t j = (this->ri)[i];
  T x = this->elements_r()[j];
  return ISNA<T>(x)? py::None() : py::oint(x);
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

template <typename T>
IntegerStats<T>* IntColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new IntegerStats<T>();
  return static_cast<IntegerStats<T>*>(stats);
}

template <typename T> T       IntColumn<T>::min() const  { return get_stats()->min(this); }
template <typename T> T       IntColumn<T>::max() const  { return get_stats()->max(this); }
template <typename T> T       IntColumn<T>::mode() const { return get_stats()->mode(this); }
template <typename T> int64_t IntColumn<T>::sum() const  { return get_stats()->sum(this); }
template <typename T> double  IntColumn<T>::mean() const { return get_stats()->mean(this); }
template <typename T> double  IntColumn<T>::sd() const   { return get_stats()->stdev(this); }
template <typename T> double  IntColumn<T>::skew() const { return get_stats()->skew(this); }
template <typename T> double  IntColumn<T>::kurt() const { return get_stats()->kurt(this); }


template <typename T>
int64_t IntColumn<T>::min_int64() const {
  T x = min();
  return ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
}

template <typename T>
int64_t IntColumn<T>::max_int64() const {
  T x = max();
  return ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
}



//------------------------------------------------------------------------------

// Explicit instantiation of the template
template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
