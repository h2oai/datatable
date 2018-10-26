//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "rowindex.h"
#include <cstring>     // std::memcpy
#include "utils.h"
#include "utils/assert.h"
#include "utils/parallel.h"


//==============================================================================
// Construction
//==============================================================================

RowIndex::RowIndex() : impl(nullptr) {}

RowIndex::RowIndex(RowIndexImpl* rii) : impl(rii) {}

// copy-constructor, performs shallow copying
RowIndex::RowIndex(const RowIndex& other) {
  impl = other.impl;
  if (impl) impl->acquire();
}

// assignment operator, performs shallow copying
RowIndex& RowIndex::operator=(const RowIndex& other) {
  if (impl) impl->release();
  impl = other.impl;
  if (impl) impl->acquire();
  return *this;
}

// move-constructor
RowIndex::RowIndex(RowIndex&& other) {
  impl = other.impl;
  other.impl = nullptr;
}

RowIndex::~RowIndex() {
  if (impl) impl->release();
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



//==============================================================================
// API
//==============================================================================

void RowIndex::clear() {
  if (impl) impl->release();
  impl = nullptr;
}


void RowIndex::shrink(size_t nrows, size_t ncols) {
  xassert(impl && impl->refcount >= ncols + 1);
  if (impl->refcount == ncols + 1) {
    impl->shrink(nrows);
  } else {
    impl->refcount--;
    impl = impl->shrunk(nrows);
    xassert(impl->refcount == 1);
  }
}


void RowIndex::extract_into(arr32_t& target) const
{
  if (!impl) return;
  size_t szlen = static_cast<size_t>(length());
  xassert(target.size() >= szlen);
  switch (impl->type) {
    case RI_ARR32: {
      std::memcpy(target.data(), indices32(), szlen * sizeof(int32_t));
      break;
    }
    case RI_SLICE: {
      if (szlen <= INT32_MAX && max() <= INT32_MAX) {
        int32_t start = static_cast<int32_t>(slice_start());
        int32_t step = static_cast<int32_t>(slice_step());
        dt::run_interleaved(
          [&](size_t i0, size_t i1, size_t di) {
            for (size_t i = i0; i < i1; i += di) {
              target[i] = start + static_cast<int32_t>(i) * step;
            }
          }, szlen);
      }
      break;
    }
    default:
      break;
  }
}


RowIndex RowIndex::uplift(const RowIndex& ri2) const {
  if (isabsent()) return RowIndex(ri2);
  if (ri2.isabsent()) return RowIndex(*this);
  return RowIndex(impl->uplift_from(ri2.impl));
}


RowIndex RowIndex::inverse(size_t nrows) const {
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


void RowIndex::verify_integrity() const {
  if (impl) impl->verify_integrity();
}


void RowIndexImpl::verify_integrity() const {
  if (length < 0) {
    throw AssertionError() << "RowIndex.length is negative: " << length;
  }
  if (refcount <= 0) {
    throw AssertionError() << "RowIndex has invalid refcount: " << refcount;
  }
  if (length == 0 && (min || max)) {
    throw AssertionError() << "RowIndex has length 0, but either min = " << min
        << " or max = " << max << " are non-zero";
  }
  if (min < 0) {
    throw AssertionError() << "min value in RowIndex is negative: " << min;
  }
  if (min > max) {
    throw AssertionError() << "min value in RowIndex is larger than max: min = "
        << min << ", max = " << max;
  }
}
