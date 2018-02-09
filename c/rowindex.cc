//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "rowindex.h"
#include <cstring>     // std::memcpy
#include "datatable_check.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/omp.h"


//==============================================================================
// Base RowIndex class
//==============================================================================

// copy-constructor, performs shallow copying
RowIndex::RowIndex(const RowIndex& other) {
  impl = other.impl;
  if (impl) impl->acquire();
}

// assignment operator, performs shallow copying
RowIndex& RowIndex::operator=(const RowIndex& other) {
  clear();
  impl = other.impl;
  if (impl) impl->acquire();
  return *this;
}

RowIndex::~RowIndex() {
  if (impl) impl->release();
}

void RowIndex::clear() {
  if (impl) impl->release();
  impl = nullptr;
}


RowIndex RowIndex::from_slice(int64_t start, int64_t count, int64_t step) {
  return RowIndex(new SliceRowIndexImpl(start, count, step));
}


RowIndex RowIndex::from_slices(const dt::array<int64_t>& starts,
                               const dt::array<int64_t>& counts,
                               const dt::array<int64_t>& steps) {
  return RowIndex(new ArrayRowIndexImpl(starts, counts, steps));
}


RowIndex RowIndex::from_array32(dt::array<int32_t>&& arr, bool sorted) {
  return RowIndex(new ArrayRowIndexImpl(std::move(arr), sorted));
}


RowIndex RowIndex::from_array64(dt::array<int64_t>&& arr, bool sorted) {
  return RowIndex(new ArrayRowIndexImpl(std::move(arr), sorted));
}


RowIndex RowIndex::from_filterfn32(filterfn32* f, int64_t n, bool sorted) {
  return RowIndex(new ArrayRowIndexImpl(f, n, sorted));
}


RowIndex RowIndex::from_filterfn64(filterfn64* f, int64_t n, bool sorted) {
  return RowIndex(new ArrayRowIndexImpl(f, n, sorted));
}


RowIndex RowIndex::from_column(Column* col) {
  return RowIndex(new ArrayRowIndexImpl(col));
}


dt::array<int32_t> RowIndex::extract_as_array32() const
{
  dt::array<int32_t> res;
  if (!impl) return res;
  size_t szlen = static_cast<size_t>(length());
  res.resize(szlen);
  switch (impl->type) {
    case RI_ARR32: {
      std::memcpy(res.data(), indices32(), szlen * sizeof(int32_t));
      break;
    }
    case RI_SLICE: {
      if (szlen <= INT32_MAX && max() <= INT32_MAX) {
        int32_t start = static_cast<int32_t>(slice_start());
        int32_t step = static_cast<int32_t>(slice_step());
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < szlen; ++i) {
          res[i] = start + static_cast<int32_t>(i) * step;
        }
      }
      break;
    }
    default:
      break;
  }
  return res;
}


RowIndex RowIndex::uplift(const RowIndex& ri2) const {
  if (isabsent()) return RowIndex(ri2);
  if (ri2.isabsent()) return RowIndex(*this);
  return RowIndex(impl->uplift_from(ri2.impl));
}


size_t RowIndex::memory_footprint() const {
  return sizeof(this) + (impl? impl->memory_footprint() : 0);
}


bool RowIndex::verify_integrity(IntegrityCheckContext& icc) const {
  return impl? impl->verify_integrity(icc) : true;
}


bool RowIndexImpl::verify_integrity(IntegrityCheckContext& icc) const {
  auto end = icc.end();

  if (length < 0) {
    icc << "RowIndex.length is negative: " << length << end;
    return false;
  }
  if (refcount <= 0) {
    icc << "RowIndex has invalid refcount: " << refcount << end;
    return false;
  }
  if (length == 0 && (min || max)) {
    icc << "RowIndex has length 0, but either min = " << min << " or max = "
        << max << " are non-zero" << end;
    return false;
  }
  if (min < 0) {
    icc << "min value in RowIndex is negative: " << min << end;
    return false;
  }
  if (min > max) {
    icc << "min value in RowIndex is larger than max: min = " << min
        << ", max = " << max << end;
    return false;
  }
  return true;
}




















//============================================================================



/**
 * Convert a slice RowIndex into an RI_ARR32/RI_ARR64.
RowIndex* RowIndex::expand()
{
  if (type != RI_SLICE) return new RowIndex(this);

  if (_length <= INT32_MAX && _max <= INT32_MAX) {
    int32_t n = (int32_t) _length;
    int32_t start = (int32_t) slice.start;
    int32_t step = (int32_t) slice.step;
    int32_t* out = (int32_t*) malloc(sizeof(int32_t) * (size_t) n);
    #pragma omp parallel for schedule(static)
    for (int32_t i = 0; i < n; ++i) {
      out[i] = start + i*step;
    }
    return new RowIndex(out, n, 1);
  } else {
    int64_t n = _length;
    int64_t start = slice.start;
    int64_t step = slice.step;
    dtdeclmalloc(out, int64_t, n);
    #pragma omp parallel for schedule(static)
    for (int64_t i = 0; i < n; i++) {
      out[i] = start + i*step;
    }
    return new RowIndex(out, n, 1);
  }
}
  */




/**
 * See DataTable::verify_integrity for method description
 */
/*
bool RowIndex::verify_integrity(IntegrityCheckContext& icc,
                const std::string& name) const
{
  int nerrors = icc.n_errors();
  auto end = icc.end();

  int64_t maxrow = -INT64_MAX;
  int64_t minrow = INT64_MAX;

  switch (type) {

    case RI_ARR32: {
      // Check that the rowindex length can be represented as an int32
      if (_length > INT32_MAX) {
        icc << name << " with type `RI_ARR32` cannot have a length greater than "
          << "INT32_MAX: length = " << _length << end;
      }

      // Check that allocation size is valid
      size_t n_allocd = array_size(ind32, sizeof(int32_t));
      if (n_allocd < static_cast<size_t>(_length)) {
        icc << name << " requires a minimum array length of " << _length
          << " elements, but only allocated enough space for " << n_allocd
          << end;
        return false;
      }

      // Check that every item in the array is a valid value
      for (int32_t i = 0; i < _length; ++i) {
        if (ind32[i] < 0) {
        icc << "Item " << i << " in " << name << " is negative: " << ind32[i]
          << end;
        }
        if (ind32[i] > maxrow) maxrow = static_cast<int64_t>(ind32[i]);
        if (ind32[i] < minrow) minrow = static_cast<int64_t>(ind32[i]);
      }
      break;
    }
    case RI_ARR64: {
      // Check that the rowindex length can be represented as an int64
      size_t n_allocd = array_size(ind64, sizeof(int64_t));
      if (n_allocd < static_cast<size_t>(_length)) {
        icc << name << " requires a minimum array length of " << _length
          << " elements, but only allocated enough space for " << n_allocd
          << end;
        return false;
      }

      // Check that every item in the array is a valid value
      for (int64_t i = 0; i < _length; ++i) {
        if (ind64[i] < 0) {
        icc << "Item " << i << " in " << name << " is negative: " << ind64[i]
          << end;
        }
        if (ind64[i] > maxrow) maxrow = ind64[i];
        if (ind64[i] < minrow) minrow = ind64[i];
      }
      break;
    }
    default: {
      icc << "Invalid type for " << name << ": " << type << end;
      return false;
    }
  };

  // Check that the found extrema coincides with the reported extrema
  if (_length == 0) minrow = maxrow = 0;
  if (this->_min != minrow) {
    icc << "Mistmatch between minimum value reported by " << name << " and "
      << "computed minimum: computed minimum is " << minrow << ", whereas "
      << "RowIndex.min = " << this->_min << end;
  }
  if (this->_max != maxrow) {
    icc << "Mistmatch between maximum value reported by " << name << " and "
      << "computed maximum: computed maximum is " << maxrow << ", whereas "
      << "RowIndex.max = " << this->_max << end;
  }

  return !icc.has_errors(nerrors);
}
*/
