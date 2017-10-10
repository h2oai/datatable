//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "column.h"


template <typename T>
FwColumn<T>::FwColumn(int64_t nrows_) : Column(nrows_)
{
  size_t sz = sizeof(T) * static_cast<size_t>(nrows_);
  mbuf = new MemoryMemBuf(sz);
}


template <typename T>
T* FwColumn<T>::elements() {
  return static_cast<T*>(mbuf->get());
}


template <typename T>
T FwColumn<T>::get_elem(int64_t i) const {
  return mbuf->get_elem<T>(i);
}


template <typename T>
void FwColumn<T>::set_elem(int64_t i, T value) {
  mbuf->set_elem<T>(i, value);
}


template <typename T>
Column* FwColumn<T>::extract_simple_slice(RowIndex* rowindex) const
{
  int64_t res_nrows = rowindex->length;
  // Column *res = Column::make_data_column(stype(), res_nrows);
  Column *res = new Column(stype(), (size_t)res_nrows);
  size_t offset = static_cast<size_t>(rowindex->slice.start) * sizeof(T);
  memcpy(res->data(), this->mbuf->at(offset), res->alloc_size());
  return res;
}


// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<PyObject*>;
