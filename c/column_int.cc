//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "utils/omp.h"
#include "py_types.h"
#include "py_utils.h"

template <typename T>
IntColumn<T>::IntColumn() : FwColumn<T>() {}

template <typename T>
IntColumn<T>::IntColumn(int64_t nrows_, MemoryBuffer* mb) :
    FwColumn<T>(nrows_, mb) {}


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
  if (!s->mean_computed()) s->mean_compute(this);
  return s->mean_get();
}

template <typename T>
double IntColumn<T>::sd() const {
  IntegerStats<T> *s = get_stats();
  if (!s->sd_computed()) s->sd_compute(this);
  return s->sd_get();
}

template <typename T>
T IntColumn<T>::min() const {
  IntegerStats<T> *s = get_stats();
  if (!s->min_computed()) s->min_compute(this);
  return s->min_get();
}

template <typename T>
T IntColumn<T>::max() const {
  IntegerStats<T> *s = get_stats();
  if (!s->max_computed()) s->max_compute(this);
  return s->max_get();
}

template <typename T>
int64_t IntColumn<T>::sum() const {
  IntegerStats<T> *s = get_stats();
  if (!s->sum_computed()) s->sum_compute(this);
  return s->sum_get();
}


// Retrieve stat value as a column
template <typename T>
Column* IntColumn<T>::min_column() const {
  IntColumn<T>* col = new IntColumn<T>(1);
  col->set_elem(0, min());
  return col;
}

template <typename T>
Column* IntColumn<T>::max_column() const {
  IntColumn<T>* col = new IntColumn<T>(1);
  col->set_elem(0, max());
  return col;
}

template <typename T>
Column* IntColumn<T>::sum_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, sum());
  return col;
}

template <typename T>
Column* IntColumn<T>::mean_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, mean());
  return col;
}

template <typename T>
Column* IntColumn<T>::sd_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, sd());
  return col;
}


//----- Type casts -------------------------------------------------------------

template<typename IT, typename OT>
inline static void cast_helper(int64_t nrows, const IT* src, OT* trg) {
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    trg[i] = ISNA<IT>(x)? GETNA<OT>() : static_cast<OT>(x);
  }
}

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
  cast_helper<T, int8_t>(this->nrows, this->elements(), target->elements());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  cast_helper<T, int16_t>(this->nrows, this->elements(), target->elements());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  cast_helper<T, int32_t>(this->nrows, this->elements(), target->elements());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  cast_helper<T, int64_t>(this->nrows, this->elements(), target->elements());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<float>* target) const {
  cast_helper<T, float>(this->nrows, this->elements(), target->elements());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<double>* target) const {
  cast_helper<T, double>(this->nrows, this->elements(), target->elements());
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
