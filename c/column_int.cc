//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include "column/sentinel_fw.h"
#include "python/int.h"
#include "datatablemodule.h"
namespace dt {



template <typename T>
ColumnImpl* IntColumn<T>::clone() const {
  return new IntColumn<T>(this->nrows_, Buffer(this->mbuf_));
}


template <typename T>
bool IntColumn<T>::get_element(size_t i, int32_t* out) const {
  T x = static_cast<const T*>(this->mbuf_.rptr())[i];
  *out = static_cast<int32_t>(x);
  return !ISNA<T>(x);
}

template <typename T>
bool IntColumn<T>::get_element(size_t i, int64_t* out) const {
  T x = static_cast<const T*>(this->mbuf_.rptr())[i];
  *out = static_cast<int64_t>(x);
  return !ISNA<T>(x);
}




//------------------------------------------------------------------------------

// Explicit instantiation of the template
template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
} // namespace dt
