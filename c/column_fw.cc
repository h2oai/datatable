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
#include "parallel/api.h"  // dt::parallel_for_static
#include "column.h"



/**
 * Private constructor that creates an "invalid" column. An `init_X` method
 * should be subsequently called before using this column.
 */
template <typename T>
FwColumn<T>::FwColumn() : ColumnImpl(0) {}

template <typename T>
FwColumn<T>::FwColumn(size_t nrows_) : ColumnImpl(nrows_) {
  mbuf.resize(sizeof(T) * nrows_);
}

template <typename T>
FwColumn<T>::FwColumn(size_t nrows_, MemoryRange&& mr) : ColumnImpl(nrows_) {
  size_t req_size = sizeof(T) * nrows_;
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
  mbuf.resize(_nrows * sizeof(T));
}



//==============================================================================

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


template <typename T>
void FwColumn<T>::materialize() {
  // If the rowindex is absent, then the column is already materialized.
  if (!ri) return;
  bool simple_slice = ri.isslice() && ri.slice_step() == 1;
  bool ascending = ri.isslice() && static_cast<int64_t>(ri.slice_step()) > 0;

  size_t elemsize = sizeof(T);
  size_t newsize = elemsize * _nrows;

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
    ri.iterate(0, _nrows, 1,
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
  if (new_nrows == _nrows) return;
  materialize();

  mbuf.resize(sizeof(T) * new_nrows);

  if (new_nrows > _nrows) {
    // Replicate the value or fill with NAs
    T fill_value = _nrows == 1? get_elem(0) : na_elem;
    T* data_dest = static_cast<T*>(mbuf.wptr());
    for (size_t i = _nrows; i < new_nrows; ++i) {
      data_dest[i] = fill_value;
    }
  }
  this->_nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
}



template <typename T>
size_t FwColumn<T>::data_nrows() const {
  return mbuf.size() / sizeof(T);
}


template <typename T>
void FwColumn<T>::apply_na_mask(const OColumn& mask) {
  xassert(mask.stype() == SType::BOOL);
  auto maskdata = static_cast<const int8_t*>(mask->data());
  T* coldata = this->elements_w();

  dt::parallel_for_static(_nrows,
    [=](size_t i) {
      if (maskdata[i] == 1) coldata[i] = GETNA<T>();
    });
  if (stats != nullptr) stats->reset();
}


template <typename T>
void FwColumn<T>::fill_na() {
  xassert(!ri);
  T* vals = static_cast<T*>(mbuf.wptr());
  dt::parallel_for_static(_nrows,
    [=](size_t i) {
      vals[i] = GETNA<T>();
    });
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
    OColumn&, const RowIndex& replace_at, const OColumn& replace_with)
{
  materialize();
  if (!replace_with) {
    return replace_values(replace_at, GETNA<T>());
  }
  OColumn with = (replace_with.stype() == _stype)
                    ? replace_with  // copy
                    : replace_with.cast(_stype);

  if (with.nrows() == 1) {
    promote<T> replace_value;
    bool isna = with.get_element(0, &replace_value);
    return isna? replace_values(replace_at, GETNA<T>()) :
                 replace_values(replace_at, downcast<T>(replace_value));
  }

  size_t replace_n = replace_at.size();
  const T* data_src = static_cast<const T*>(with->data());
  const RowIndex& rowindex_src = with->rowindex();
  T* data_dest = elements_w();
  xassert(with.nrows() == replace_n);
  if (rowindex_src) {
    replace_at.iterate(0, replace_n, 1,
      [&](size_t i, size_t j) {
        if (j == RowIndex::NA) return;
        size_t k = rowindex_src[i];
        data_dest[j] = (k == RowIndex::NA)? GETNA<T>() : data_src[k];
      });
  } else {
    replace_at.iterate(0, replace_n, 1,
      [&](size_t i, size_t j) {
        if (j == RowIndex::NA) return;
        data_dest[j] = data_src[i];
      });
  }
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
