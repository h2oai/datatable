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
#include "utils/omp.h"
#include "py_types.h"
#include "py_utils.h"


template <typename T>
IntColumn<T>::~IntColumn() {}


template <typename T>
SType IntColumn<T>::stype() const {
  return stype_integer(sizeof(T));
}

//---- Stats -------------------------------------------------------------------

template <typename T>
IntegerStats<T>* IntColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new IntegerStats<T>();
  return static_cast<IntegerStats<T>*>(stats);
}

// Retrieve stat value
template <typename T>
double IntColumn<T>::mean() const {
  IntegerStats<T> *s = get_stats();
  if (!s->mean_computed()) s->compute_mean(this);
  return s->_mean;
}

template <typename T>
double IntColumn<T>::sd() const {
  IntegerStats<T> *s = get_stats();
  if (!s->sd_computed()) s->compute_sd(this);
  return s->_sd;
}

template <typename T>
T IntColumn<T>::min() const {
  IntegerStats<T> *s = get_stats();
  if (!s->min_computed()) s->compute_min(this);
  return s->_min;
}

template <typename T>
T IntColumn<T>::max() const {
  IntegerStats<T> *s = get_stats();
  if (!s->max_computed()) s->compute_max(this);
  return s->_max;
}

template <typename T>
int64_t IntColumn<T>::sum() const {
  IntegerStats<T> *s = get_stats();
  if (!s->sum_computed()) s->compute_sum(this);
  return s->_sum;
}


// Retrieve stat value as a column
template <typename T>
Column* IntColumn<T>::min_column() const {
  IntColumn<T>* col =
      static_cast<IntColumn<T>*>(new_data_column(stype_integer(sizeof(T)), 1));
  col->set_elem(0, min());
  return col;
}

template <typename T>
Column* IntColumn<T>::max_column() const {
  IntColumn<T>* col =
      static_cast<IntColumn<T>*>(new_data_column(stype_integer(sizeof(T)), 1));
  col->set_elem(0, max());
  return col;
}

template <typename T>
Column* IntColumn<T>::sum_column() const {
  IntColumn<int64_t>* col =
      static_cast<IntColumn<int64_t>*>(new_data_column(ST_INTEGER_I8, 1));
  col->set_elem(0, sum());
  return col;
}

template <typename T>
Column* IntColumn<T>::mean_column() const {
  RealColumn<double>* col =
      static_cast<RealColumn<double>*>(new_data_column(ST_REAL_F8, 1));
  col->set_elem(0, mean());
  return col;
}

template <typename T>
Column* IntColumn<T>::sd_column() const {
  RealColumn<double>* col =
      static_cast<RealColumn<double>*>(new_data_column(ST_REAL_F8, 1));
  col->set_elem(0, sd());
  return col;
}


//----- Type casts -------------------------------------------------------------

template <typename T>
void IntColumn<T>::cast_into(BoolColumn* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int8_t na_trg = GETNA<int8_t>();
  T* src_data = this->elements();
  int8_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : (x != 0);
  }
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int8_t>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int8_t na_trg = GETNA<int8_t>();
  T* src_data = this->elements();
  int8_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<int8_t>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int16_t na_trg = GETNA<int16_t>();
  T* src_data = this->elements();
  int16_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<int16_t>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int32_t na_trg = GETNA<int32_t>();
  T* src_data = this->elements();
  int32_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<int32_t>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int64_t na_trg = GETNA<int64_t>();
  T* src_data = this->elements();
  int64_t* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<int64_t>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<float>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr float na_trg = GETNA<float>();
  T* src_data = this->elements();
  float* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<float>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<double>* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr double na_trg = GETNA<double>();
  T* src_data = this->elements();
  double* trg_data = target->elements();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : static_cast<double>(x);
  }
}

template <typename T>
void IntColumn<T>::cast_into(PyObjectColumn* target) const {
  constexpr T na_src = GETNA<T>();
  T* src_data = this->elements();
  PyObject** trg_data = target->elements();
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    // PyLong_FromInt64 is declared in "py_types.h" as an alias for either
    // PyLong_FromLong or PyLong_FromLongLong, depending on the platform
    trg_data[i] = x == na_src ? none() :
                  (sizeof(T) == 8? PyLong_FromInt64(x) :
                                   PyLong_FromLong(static_cast<long>(x)));
  }
}

template <>
void IntColumn<int8_t>::cast_into(IntColumn<int8_t>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}

template <>
void IntColumn<int16_t>::cast_into(IntColumn<int16_t>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}

template <>
void IntColumn<int32_t>::cast_into(IntColumn<int32_t>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}

template <>
void IntColumn<int64_t>::cast_into(IntColumn<int64_t>* target) const {
  memcpy(target->data(), this->data(), alloc_size());
}



//------------------------------------------------------------------------------

// Explicit instantiation of the template
template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
