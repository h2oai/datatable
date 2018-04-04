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
  clear(false);
  impl = other.impl;
  if (impl) impl->acquire();
  return *this;
}

RowIndex::~RowIndex() {
  if (impl) impl->release();
}

void RowIndex::clear(bool keep_groups) {
  if (keep_groups && impl && impl->groups) {
    RowIndexImpl* new_impl = new SliceRowIndexImpl(0, impl->length, 1);
    swap(new_impl->groups, impl->groups);
    impl->release();
    impl = new_impl;
  } else {
    if (impl) impl->release();
    impl = nullptr;
  }
}


RowIndex RowIndex::from_slice(int64_t start, int64_t count, int64_t step) {
  return RowIndex(new SliceRowIndexImpl(start, count, step));
}


RowIndex RowIndex::from_slices(const arr64_t& starts,
                               const arr64_t& counts,
                               const arr64_t& steps) {
  return RowIndex(new ArrayRowIndexImpl(starts, counts, steps));
}


RowIndex RowIndex::from_array32(arr32_t&& arr, bool sorted) {
  return RowIndex(new ArrayRowIndexImpl(std::move(arr), sorted));
}


RowIndex RowIndex::from_array64(arr64_t&& arr, bool sorted) {
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


size_t RowIndex::get_ngroups() const {
  if (!impl || !impl->groups) return 0;
  return impl->groups.size() - 1;
}


arr32_t RowIndex::extract_as_array32() const
{
  arr32_t res;
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


RowIndex RowIndex::inverse(int64_t nrows) const {
  if (isabsent()) {
    // No RowIndex is equivalent to having RowIndex over all rows. The inverse
    // of that is a 0-length RowIndex.
    return RowIndex(new SliceRowIndexImpl(0, 0, 0));
  }
  if (length() == 0) {
    // An inverse of a 0-length RowIndex is a RowIndex over all rows, which we
    // return as an empty RowIndex object.
    return RowIndex();
  }
  if (nrows < max()) {
    throw ValueError() << "Invalid nrows=" << nrows << " for a RowIndex with "
                          "largest index " << max();
  }
  return RowIndex(impl->inverse(nrows));
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
