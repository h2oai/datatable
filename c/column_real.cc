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
#include "myomp.h"
#include "py_utils.h"


template <typename T>
RealColumn<T>::~RealColumn() {}


template <typename T>
SType RealColumn<T>::stype() const {
  return stype_real(sizeof(T));
}

//---- Stats -------------------------------------------------------------------

template <typename T>
RealStats<T>* RealColumn<T>::get_stats() {
  if (stats == nullptr) stats = new RealStats<T>(this);
  return static_cast<RealStats<T>*>(stats);
}

// Retrieve stat value
template <typename T>
double RealColumn<T>::mean() {
  RealStats<T> *s = get_stats();
  if (!s->mean_computed()) s->compute_mean();
  return s->_mean;
}

template <typename T>
double RealColumn<T>::sd() {
  RealStats<T> *s = get_stats();
  if (!s->sd_computed()) s->compute_sd();
  return s->_sd;
}

template <typename T>
T RealColumn<T>::min() {
  RealStats<T> *s = get_stats();
  if (!s->min_computed()) s->compute_min();
  return s->_min;
}

template <typename T>
T RealColumn<T>::max() {
  RealStats<T> *s = get_stats();
  if (!s->max_computed()) s->compute_max();
  return s->_max;
}

template <typename T>
double RealColumn<T>::sum() {
  RealStats<T> *s = get_stats();
  if (!s->sum_computed()) s->compute_sum();
  return s->_sum;
}


// Retrieve stat value as a column
template <typename T>
Column* RealColumn<T>::min_column() {
  Column* col = new_data_column(stype(), 1);
  ((T*) col->data())[0] = min();
  return col;
}

template <typename T>
Column* RealColumn<T>::max_column() {
  Column* col = new_data_column(stype(), 1);
  ((T*) col->data())[0] = max();
  return col;
}

template <typename T>
Column* RealColumn<T>::sum_column() {
  Column* col = new_data_column(ST_REAL_F8, 1);
  ((double*) col->data())[0] = sum();
  return col;
}

template <typename T>
Column* RealColumn<T>::mean_column() {
  Column* col = new_data_column(ST_REAL_F8, 1);
  ((double*) col->data())[0] = mean();
  return col;
}

template <typename T>
Column* RealColumn<T>::sd_column() {
  Column* col = new_data_column(ST_REAL_F8, 1);
  ((double*) col->data())[0] = sd();
  return col;
}


//----- Type casts -------------------------------------------------------------

template <typename T>
void RealColumn<T>::cast_into(BoolColumn* target) const {
  constexpr int8_t na_trg = GETNA<int8_t>();
  T* src_data = this->elements();
  int8_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : (x != 0);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int8_t>* target) const {
  constexpr int8_t na_trg = GETNA<int8_t>();
  T* src_data = this->elements();
  int8_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : static_cast<int8_t>(x);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  constexpr int16_t na_trg = GETNA<int16_t>();
  T* src_data = this->elements();
  int16_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : static_cast<int16_t>(x);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  constexpr int32_t na_trg = GETNA<int32_t>();
  T* src_data = this->elements();
  int32_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : static_cast<int32_t>(x);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  constexpr int64_t na_trg = GETNA<int64_t>();
  T* src_data = this->elements();
  int64_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : static_cast<int64_t>(x);
  }
}

template <>
void RealColumn<float>::cast_into(RealColumn<double>* target) const {
  float* src_data = this->elements();
  double* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    trg_data[i] = static_cast<double>(src_data[i]);
  }
}

template <>
void RealColumn<double>::cast_into(RealColumn<float>* target) const {
  double* src_data = this->elements();
  float* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    trg_data[i] = static_cast<float>(src_data[i]);
  }
}

template <>
void RealColumn<float>::cast_into(RealColumn<float>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}

template <>
void RealColumn<double>::cast_into(RealColumn<double>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}

template <typename T>
void RealColumn<T>::cast_into(PyObjectColumn* target) const {
  T* src_data = this->elements();
  PyObject** trg_data = target->elements();
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? none()
                            : PyFloat_FromDouble(static_cast<double>(x));
  }
}




//------------------------------------------------------------------------------


// Explicit instantiation of the template
template class RealColumn<float>;
template class RealColumn<double>;
