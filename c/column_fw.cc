//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <type_traits>
#include "column.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/omp.h"



/**
 * Private constructor that creates an "invalid" column. An `init_X` method
 * should be subsequently called before using this column.
 */
template <typename T>
FwColumn<T>::FwColumn() : Column(0) {}

template <typename T>
FwColumn<T>::FwColumn(int64_t nrows_) : Column(nrows_) {}

template <typename T>
FwColumn<T>::FwColumn(int64_t nrows_, MemoryRange&& mr) : Column(nrows_) {
  size_t req_size = elemsize() * static_cast<size_t>(nrows_);
  if (mr) {
    xassert(mr.size() == req_size);
  } else {
    mr.resize(req_size);
  }
  mbuf = std::move(mr);
}


//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void FwColumn<T>::init_data() {
  xassert(!ri);
  mbuf.resize(static_cast<size_t>(nrows) * elemsize());
}

template <typename T>
void FwColumn<T>::init_mmap(const std::string& filename) {
  xassert(!ri);
  mbuf = MemoryRange(static_cast<size_t>(nrows) * elemsize(), filename);
}

template <typename T>
void FwColumn<T>::open_mmap(const std::string& filename) {
  xassert(!ri);
  mbuf = MemoryRange(filename);
  size_t exp_size = static_cast<size_t>(nrows) * sizeof(T);
  if (mbuf.size() != exp_size) {
    throw Error() << "File \"" << filename <<
        "\" cannot be used to create a column with " << nrows <<
        " rows. Expected file size of " << exp_size <<
        " bytes, actual size is " << mbuf.size() << " bytes";
  }
}

template <typename T>
void FwColumn<T>::init_xbuf(Py_buffer* pybuffer) {
  xassert(!ri);
  size_t exp_buf_len = static_cast<size_t>(nrows) * elemsize();
  if (static_cast<size_t>(pybuffer->len) != exp_buf_len) {
    throw Error() << "PyBuffer cannot be used to create a column of " << nrows
                  << " rows: buffer length is "
                  << static_cast<size_t>(pybuffer->len)
                  << ", expected " << exp_buf_len;
  }
  const void* ptr = pybuffer->buf;
  mbuf = MemoryRange(exp_buf_len, ptr, pybuffer);
}



//==============================================================================

template <typename T>
void FwColumn<T>::replace_buffer(MemoryRange&& new_mbuf, MemoryBuffer*) {
  xassert(new_mbuf);
  if (new_mbuf.size() % sizeof(T)) {
    throw RuntimeError() << "New buffer has invalid size " << new_mbuf.size();
  }
  mbuf = std::move(new_mbuf);
  nrows = static_cast<int64_t>(mbuf.size() / sizeof(T));
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
const T* FwColumn<T>::elements_r() const {
  return static_cast<const T*>(mbuf.rptr());
}

template <typename T>
T* FwColumn<T>::elements_w() {
  return static_cast<T*>(mbuf.wptr());
}


template <typename T>
T FwColumn<T>::get_elem(int64_t i) const {
  return static_cast<const T*>(mbuf.rptr())[i];
}


template <>
void FwColumn<PyObject*>::set_elem(int64_t i, PyObject* value) {
  PyObject** data = static_cast<PyObject**>(mbuf.wptr());
  data[i] = value;
  Py_INCREF(value);
}

template <typename T>
void FwColumn<T>::set_elem(int64_t i, T value) {
  T* data = static_cast<T*>(mbuf.wptr());
  data[i] = value;
}


template <typename T>
void FwColumn<T>::reify() {
  // If the rowindex is absent, then the column is already materialized.
  if (ri.isabsent()) return;
  bool simple_slice = ri.isslice() && ri.slice_step() == 1;
  bool ascending    = ri.isslice() && ri.slice_step() > 0;

  size_t elemsize = sizeof(T);
  size_t newsize = elemsize * static_cast<size_t>(nrows);

  // Current `mbuf` can be reused iff it is not readonly. Thus, `new_mbuf` can
  // be either the same as `mbuf` (with old size), or a newly allocated buffer
  // (with new size). Correspondingly, the old buffer may or may not have to be
  // released afterwards.
  // Note also that `newsize` may be either smaller or bigger than the old size,
  // this must be taken into consideration.
  MemoryRange newmr;

  if (simple_slice) {
    // Slice with step 1: a portion of the buffer can be simply mem-copied onto
    // the new buffer.
    size_t start = static_cast<size_t>(ri.slice_start());
    const void* src = mbuf.rptr(start * elemsize);
    void* dest = mbuf.is_writable()
        ? mbuf.wptr()
        : newmr.resize(newsize).wptr();
    std::memmove(dest, src, newsize);

  } else {
    // In all other cases we have to manually loop over the rowindex and
    // copy array elements onto the new positions. This can be done in-place
    // only if we know that the indices are monotonically increasing (otherwise
    // there is a risk of scrambling the data).
    const T* data_src = static_cast<const T*>(mbuf.rptr());
    T* data_dest = mbuf.is_writable() && ascending
       ? static_cast<T*>(mbuf.wptr())
       : static_cast<T*>(newmr.resize(newsize).wptr());
    ri.strided_loop(0, nrows, 1,
      [&](int64_t i) {
        *data_dest = data_src[i];
        ++data_dest;
      });
  }

  if (newmr) {
    mbuf = std::move(newmr);
  } else {
    mbuf.resize(newsize);
  }
  ri.clear(true);
}



template <typename T>
void FwColumn<T>::resize_and_fill(int64_t new_nrows)
{
  if (new_nrows == nrows) return;

  mbuf.resize(sizeof(T) * static_cast<size_t>(new_nrows));

  if (new_nrows > nrows) {
    // Replicate the value or fill with NAs
    T fill_value = nrows == 1? get_elem(0) : na_elem;
    T* data_dest = static_cast<T*>(mbuf.wptr());
    for (int64_t i = nrows; i < new_nrows; ++i) {
      data_dest[i] = fill_value;
    }
  }
  this->nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
}


template <typename T>
void FwColumn<T>::rbind_impl(std::vector<const Column*>& columns,
                             int64_t new_nrows, bool col_empty)
{
  const T na = na_elem;
  const void* naptr = static_cast<const void*>(&na);

  // Reallocate the column's data buffer
  size_t old_nrows = static_cast<size_t>(nrows);
  size_t old_alloc_size = alloc_size();
  size_t new_alloc_size = sizeof(T) * static_cast<size_t>(new_nrows);
  mbuf.resize(new_alloc_size);
  nrows = new_nrows;

  // Copy the data
  char* resptr = static_cast<char*>(mbuf.wptr());
  char* resptr0 = resptr;
  size_t rows_to_fill = 0;
  if (col_empty) {
    resptr += old_alloc_size;
    rows_to_fill = old_nrows;
  }
  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) {
      rows_to_fill += static_cast<size_t>(col->nrows);
    } else {
      if (rows_to_fill) {
        set_value(resptr, naptr, sizeof(T), rows_to_fill);
        resptr += rows_to_fill * sizeof(T);
        rows_to_fill = 0;
      }
      if (col->stype() != stype()) {
        Column* newcol = col->cast(stype());
        delete col;
        col = newcol;
      }
      std::memcpy(resptr, col->data(), col->alloc_size());
      resptr += col->alloc_size();
    }
    delete col;
  }
  if (rows_to_fill) {
    set_value(resptr, naptr, sizeof(T), rows_to_fill);
    resptr += rows_to_fill * sizeof(T);
  }
  xassert(resptr == resptr0 + new_alloc_size);
  (void)resptr0;
}


template <typename T>
int64_t FwColumn<T>::data_nrows() const {
  return static_cast<int64_t>(mbuf.size() / sizeof(T));
}


template <typename T>
void FwColumn<T>::apply_na_mask(const BoolColumn* mask) {
  const int8_t* maskdata = mask->elements_r();
  constexpr T na = GETNA<T>();
  T* coldata = this->elements_w();
  #pragma omp parallel for schedule(dynamic, 1024)
  for (int64_t j = 0; j < nrows; ++j) {
    if (maskdata[j] == 1) coldata[j] = na;
  }
  if (stats != nullptr) stats->reset();
}


template <typename T>
void FwColumn<T>::fill_na() {
  T* vals = static_cast<T*>(mbuf.wptr());
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    vals[i] = GETNA<T>();
  }
  ri.clear(false);
}


template <typename T>
void FwColumn<T>::replace_values(
    RowIndex replace_at, const Column* replace_with)
{
  if (replace_with->stype() != stype()) {
    replace_with = replace_with->cast(stype());
  }

  int64_t replace_n = replace_at.length();
  const T* data_src = static_cast<const T*>(replace_with->data());
  T* data_dest = elements_w();
  if (replace_with->nrows == 1) {
    T value = *data_src;
    replace_at.strided_loop(0, replace_n, 1,
      [&](int64_t i) {
        data_dest[i] = value;
      });
  } else {
    xassert(replace_with->nrows == replace_n);
    replace_at.strided_loop(0, replace_n, 1,
      [&](int64_t i) {
        data_dest[i] = *data_src;
        ++data_src;
      });
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
