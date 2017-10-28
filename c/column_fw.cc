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
  mbuf = sz? new MemoryMemBuf(sz) : nullptr;
}

template <typename T>
void FwColumn<T>::replace_buffer(MemoryBuffer* new_mbuf, MemoryBuffer*) {
  if (new_mbuf->size() % sizeof(T)) {
    throw RuntimeError() << "New buffer has invalid size " << new_mbuf->size();
  }
  MemoryBuffer* t = new_mbuf->shallowcopy();
  if (mbuf) mbuf->release();
  mbuf = t;
  nrows = static_cast<int64_t>(mbuf->size() / sizeof(T));
}

template <typename T>
size_t FwColumn<T>::elemsize() const {
  return sizeof(T);
}

template <typename T>
bool FwColumn<T>::is_fixedwidth() const {
  return true;
}


template <typename T>
T* FwColumn<T>::elements() const {
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
void FwColumn<T>::reify() {
  // If our rowindex is null, then we're already done
  if (ri == nullptr) return;

  size_t elemsize = sizeof(T);
  size_t nrows_cast = static_cast<size_t>(nrows);
  size_t newsize = elemsize * nrows_cast;

  // Current `mbuf` can be reused iff it is not readonly. Thus, `new_mbuf` can
  // be either the same as `mbuf` (with old size), or a newly allocated buffer
  // (with new size). Correspondingly, the old buffer may or may not have to be
  // released afterwards.
  // Note also that `newsize` may be either smaller or bigger than the old size,
  // this must be taken into consideration.
  auto new_mbuf = mbuf->is_readonly()? new MemoryMemBuf(newsize) : mbuf;

  if (ri->type == RI_SLICE && ri->slice.step == 1) {
    // Slice with step 1: a portion of the buffer can be simply mem-moved onto
    // the new buffer (use memmove because the old and the new buffer can be
    // the same).
    assert(newsize + ri->slice.start*elemsize <= mbuf->size());
    memmove(new_mbuf->get(), elements() + ri->slice.start, newsize);

  } else {
    // In all other cases we have to manually loop over the rowindex and
    // copy array elements onto the new positions. This can be done in-place
    // only if we know that the indices are monotonically increasing (otherwise
    // there is a risk of scrambling the data).
    if (mbuf == new_mbuf && !(ri->type == RI_SLICE && ri->slice.step > 0)) {
      new_mbuf = new MemoryMemBuf(newsize);
    }
    T* data_src = elements();
    T* data_dest = static_cast<T*>(new_mbuf->get());
    DT_LOOP_OVER_ROWINDEX(i, nrows, ri,
      *data_dest = data_src[i];
      ++data_dest;
    )
  }

  if (mbuf == new_mbuf) {
    new_mbuf->resize(newsize);
  } else {
    mbuf->release();
    mbuf = new_mbuf;
  }
  ri->release();
  ri = nullptr;
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
    throw RuntimeError() << "Column::resize_and_fill() cannot shrink a column";
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
  if (this->stats != nullptr) this->stats->reset();
}


template <typename T>
void FwColumn<T>::rbind_impl(const std::vector<const Column*>& columns,
                             int64_t new_nrows, bool col_empty)
{
    const T na = na_elem;
    const void *naptr = static_cast<const void*>(&na);

    // Reallocate the column's data buffer
    size_t old_nrows = (size_t) nrows;
    size_t old_alloc_size = alloc_size();
    size_t new_alloc_size = sizeof(T) * (size_t) new_nrows;
    mbuf->resize(new_alloc_size);
    nrows = new_nrows;

    // Copy the data
    void *resptr = mbuf->at(col_empty ? 0 : old_alloc_size);
    size_t rows_to_fill = col_empty ? old_nrows : 0;
    for (const Column* col : columns) {
        if (col->stype() == 0) {
            rows_to_fill += (size_t) col->nrows;
        } else {
            if (rows_to_fill) {
                set_value(resptr, naptr, sizeof(T), rows_to_fill);
                resptr = add_ptr(resptr, rows_to_fill * sizeof(T));
                rows_to_fill = 0;
            }
            if (col->stype() != stype()) {
                Column *newcol = col->cast(stype());
                delete col;
                col = newcol;
            }
            memcpy(resptr, col->data(), col->alloc_size());
            resptr = add_ptr(resptr, col->alloc_size());
        }
        delete col;
    }
    if (rows_to_fill) {
        set_value(resptr, naptr, sizeof(T), rows_to_fill);
        resptr = add_ptr(resptr, rows_to_fill * sizeof(T));
    }
    assert(resptr == mbuf->at(new_alloc_size));
}


template <typename T>
int64_t FwColumn<T>::data_nrows() const {
  return static_cast<int64_t>(mbuf->size() / sizeof(T));
}


template <typename T>
void FwColumn<T>::apply_na_mask(const BoolColumn* mask) {
  const int8_t* maskdata = mask->elements();
  constexpr T na = GETNA<T>();
  T* coldata = this->elements();
  #pragma omp parallel for schedule(dynamic, 1024)
  for (int64_t j = 0; j < nrows; ++j) {
    if (maskdata[j] == 1) coldata[j] = na;
  }
  if (stats != nullptr) stats->reset();
}

template <typename T>
void FwColumn<T>::fill_na() {
  int64_t mbuf_nrows = data_nrows();
  T* data = elements();
  for (int i = 0; i < mbuf_nrows; ++i) {
    data[i] = GETNA<T>();
  }
}


// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<PyObject*>;
