//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <type_traits>
#include "utils/assert.h"
#include "utils/misc.h"
#include "utils/parallel.h"  // dt::run_parallel
#include "column.h"



/**
 * Private constructor that creates an "invalid" column. An `init_X` method
 * should be subsequently called before using this column.
 */
template <typename T>
FwColumn<T>::FwColumn() : Column(0) {}

template <typename T>
FwColumn<T>::FwColumn(size_t nrows_) : Column(nrows_) {
  mbuf.resize(elemsize() * nrows_);
}

template <typename T>
FwColumn<T>::FwColumn(size_t nrows_, MemoryRange&& mr) : Column(nrows_) {
  size_t req_size = elemsize() * nrows_;
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
  mbuf.resize(nrows * elemsize());
}

template <typename T>
void FwColumn<T>::init_mmap(const std::string& filename) {
  xassert(!ri);
  mbuf = MemoryRange::mmap(filename, nrows * elemsize());
}

template <typename T>
void FwColumn<T>::open_mmap(const std::string& filename, bool) {
  xassert(!ri);
  mbuf = MemoryRange::mmap(filename);
  // size_t exp_size = nrows * sizeof(T);
  // if (mbuf.size() != exp_size) {
  //   throw Error() << "File \"" << filename <<
  //       "\" cannot be used to create a column with " << nrows <<
  //       " rows. Expected file size of " << exp_size <<
  //       " bytes, actual size is " << mbuf.size() << " bytes";
  // }
}

template <typename T>
void FwColumn<T>::init_xbuf(Py_buffer* pybuffer) {
  xassert(!ri);
  size_t exp_buf_len = nrows * elemsize();
  if (static_cast<size_t>(pybuffer->len) != exp_buf_len) {
    throw Error() << "PyBuffer cannot be used to create a column of " << nrows
                  << " rows: buffer length is "
                  << static_cast<size_t>(pybuffer->len)
                  << ", expected " << exp_buf_len;
  }
  const void* ptr = pybuffer->buf;
  mbuf = MemoryRange::external(ptr, exp_buf_len, pybuffer);
}



//==============================================================================

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
  if (ri) materialize();
  return static_cast<T*>(mbuf.wptr());
}


template <typename T>
T FwColumn<T>::get_elem(size_t i) const {
  return static_cast<const T*>(mbuf.rptr())[i];
}


template <>
void FwColumn<PyObject*>::set_elem(size_t i, PyObject* value) {
  PyObject** data = static_cast<PyObject**>(mbuf.wptr());
  data[i] = value;
  Py_INCREF(value);
}

template <typename T>
void FwColumn<T>::set_elem(size_t i, T value) {
  T* data = static_cast<T*>(mbuf.wptr());
  data[i] = value;
}


template <typename T>
void FwColumn<T>::materialize() {
  // If the rowindex is absent, then the column is already materialized.
  if (!ri) return;
  bool simple_slice = ri.isslice() && ri.slice_step() == 1;
  bool ascending = ri.isslice() && static_cast<int64_t>(ri.slice_step()) > 0;

  size_t elemsize = sizeof(T);
  size_t newsize = elemsize * nrows;

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
    ri.iterate(0, nrows, 1,
      [&](size_t i, size_t j) {
        data_dest[i] = (j == RowIndex::NA)? GETNA<T>() : data_src[j];
      });
  }

  if (newmr) {
    mbuf = std::move(newmr);
  } else {
    mbuf.resize(newsize);
  }
  ri.clear();
}



template <typename T>
void FwColumn<T>::resize_and_fill(size_t new_nrows)
{
  if (new_nrows == nrows) return;
  materialize();

  mbuf.resize(sizeof(T) * new_nrows);

  if (new_nrows > nrows) {
    // Replicate the value or fill with NAs
    T fill_value = nrows == 1? get_elem(0) : na_elem;
    T* data_dest = static_cast<T*>(mbuf.wptr());
    for (size_t i = nrows; i < new_nrows; ++i) {
      data_dest[i] = fill_value;
    }
  }
  this->nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
}



template <typename T>
size_t FwColumn<T>::data_nrows() const {
  return mbuf.size() / sizeof(T);
}


template <typename T>
void FwColumn<T>::apply_na_mask(const BoolColumn* mask) {
  const int8_t* maskdata = mask->elements_r();
  T* coldata = this->elements_w();

  dt::run_parallel(
    [=](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) {
        if (maskdata[i] == 1) coldata[i] = GETNA<T>();
      }
    }, nrows);
  if (stats != nullptr) stats->reset();
}


template <typename T>
void FwColumn<T>::fill_na() {
  xassert(!ri);
  T* vals = static_cast<T*>(mbuf.wptr());
  dt::run_parallel(
    [=](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) {
        vals[i] = GETNA<T>();
      }
    }, nrows);
}


template <typename T>
void FwColumn<T>::replace_values(const RowIndex& replace_at, T replace_with) {
  T* data = elements_w();  // This will materialize the column if necessary
  replace_at.iterate(0, replace_at.size(), 1,
    [&](size_t, size_t j) {
      data[j] = replace_with;
    });
  if (stats) stats->reset();
}


template <typename T>
void FwColumn<T>::replace_values(
    RowIndex replace_at, const Column* replace_with)
{
  materialize();
  if (!replace_with) {
    return replace_values(replace_at, GETNA<T>());
  }
  if (replace_with->stype() != stype()) {
    replace_with = replace_with->cast(stype());
  }

  if (replace_with->nrows == 1) {
    auto rcol = dynamic_cast<const FwColumn<T>*>(replace_with);
    xassert(rcol);
    replace_values(replace_at, rcol->get_elem(0));
  }
  else {
    size_t replace_n = replace_at.size();
    const T* data_src = static_cast<const T*>(replace_with->data());
    T* data_dest = elements_w();
    xassert(replace_with->nrows == replace_n);
    replace_at.iterate(0, replace_n, 1,
      [&](size_t i, size_t j) {
        xassert(j != RowIndex::NA);
        data_dest[j] = data_src[i];
      });
  }
}


template <typename T>
static int32_t binsearch(const T* data, int32_t len, T value) {
  // Use unsigned indices in order to avoid overflows
  uint32_t start = 0;
  uint32_t end   = static_cast<uint32_t>(len);
  while (end - start > 1) {
    uint32_t mid = (start + end) >> 1;
    if (data[mid] > value) end = mid;
    else start = mid;
  }
  return (data[start] == value)? static_cast<int32_t>(start) : -1;
}


template <typename T>
RowIndex FwColumn<T>::join(const Column* keycol) const {
  xassert(stype() == keycol->stype());

  auto kcol = static_cast<const FwColumn<T>*>(keycol);
  xassert(!kcol->ri);

  arr32_t target_indices(nrows);
  int32_t* trg_indices = target_indices.data();
  const T* src_data = elements_r();
  const T* search_data = kcol->elements_r();
  int32_t search_n = static_cast<int32_t>(keycol->nrows);

  ri.iterate(0, nrows, 1,
    [&](size_t i, size_t j) {
      if (j == RowIndex::NA) return;
      T value = src_data[j];
      trg_indices[i] = binsearch<T>(search_data, search_n, value);
    });

  return RowIndex(std::move(target_indices));
}


template <typename T>
void FwColumn<T>::fill_na_mask(int8_t* outmask, size_t row0, size_t row1) {
  const T* tdata = elements_r();
  ri.iterate(row0, row1, 1,
    [&](size_t i, size_t j) {
      outmask[i] = (j == RowIndex::NA) || ISNA<T>(tdata[j]);
    });
}


// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<PyObject*>;
