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
#include <type_traits>
#include "utils.h"


template <typename T> FwColumn<T>::FwColumn() : FwColumn<T>(0) {}


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


template <>
void FwColumn<PyObject*>::set_elem(int64_t i, PyObject* value) {
  mbuf->set_elem<PyObject*>(i, value);
  Py_XINCREF(value);
}

template <typename T>
void FwColumn<T>::set_elem(int64_t i, T value) {
  mbuf->set_elem<T>(i, value);
}


template <typename T>
Column* FwColumn<T>::extract_simple_slice(RowIndex* rowindex) const
{
  int64_t res_nrows = rowindex->length;
  Column *res = Column::new_data_column(stype(), res_nrows);
  size_t offset = static_cast<size_t>(rowindex->slice.start) * sizeof(T);
  memcpy(res->data(), this->mbuf->at(offset), res->alloc_size());
  return res;
}


// The purpose of this template is to augment the behavior of the template
// `resize_and_fill` method, for the case when `T` is `PyObject*`: if a
// `PyObject*` value is replicated multiple times in a column, then its
// refcount has to be increased that many times.
template <typename T> void incr_refcnt(T, int64_t) {}
template <> void incr_refcnt(PyObject* obj, int64_t drefcnt) {
  if (obj) Py_REFCNT(obj) += drefcnt;
}


template <typename T>
void FwColumn<T>::resize_and_fill(int64_t new_nrows)
{
  if (new_nrows == nrows) return;
  if (new_nrows < nrows) {
    throw Error("Column::resize_and_fill() cannot shrink a column");
  }

  mbuf = mbuf->safe_resize(sizeof(T) * static_cast<size_t>(new_nrows));

  // Replicate the value or fill with NAs
  T fill_value = nrows == 1? get_elem(0) : na_elem;
  for (int64_t i = nrows; i < new_nrows; ++i) {
    mbuf->set_elem<T>(i, fill_value);
  }
  incr_refcnt<T>(fill_value, new_nrows - nrows);
  this->nrows = new_nrows;

  // TODO(#301): Temporary fix.
  this->stats->reset();
}


template <typename T>
int64_t FwColumn<T>::data_nrows() const {
  return static_cast<int64_t>(mbuf->size() / sizeof(T));
}


// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<PyObject*>;
