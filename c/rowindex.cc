//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <cstring>     // std::memcpy
#include "rowindex.h"
#include "rowindex_impl.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/parallel.h"


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

RowIndex::RowIndex() : impl(nullptr) {}

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


// Private constructor
RowIndex::RowIndex(RowIndexImpl* rii) {
  impl = rii;
  if (impl) impl->acquire();
}


RowIndex RowIndex::from_slice(size_t start, size_t count, size_t step) {
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



//------------------------------------------------------------------------------
// API
//------------------------------------------------------------------------------

RowIndexType RowIndex::type() const {
  return impl? impl->type : RowIndexType::UNKNOWN;
}
bool RowIndex::isabsent() const { return impl == nullptr; }
bool RowIndex::isslice() const { return impl && impl->type == RowIndexType::SLICE; }
bool RowIndex::isarr32() const { return impl && impl->type == RowIndexType::ARR32; }
bool RowIndex::isarr64() const { return impl && impl->type == RowIndexType::ARR64; }
bool RowIndex::isarray() const { return isarr32() || isarr64(); }
const void* RowIndex::ptr() const { return static_cast<const void*>(impl); }

size_t RowIndex::length() const { return impl? impl->length : 0; }
size_t RowIndex::min() const { return impl? impl->min : 0; }
size_t RowIndex::max() const { return impl? impl->max : 0; }
size_t RowIndex::nth(size_t i) const { return impl? impl->nth(i) : i; }

const int32_t* RowIndex::indices32() const {
  return static_cast<ArrayRowIndexImpl*>(impl)->indices32();
}
const int64_t* RowIndex::indices64() const {
  return static_cast<ArrayRowIndexImpl*>(impl)->indices64();
}

size_t RowIndex::slice_start() const {
  return slice_rowindex_get_start(impl);
}
size_t RowIndex::slice_step() const {
  return slice_rowindex_get_step(impl);
}



void RowIndex::clear() {
  if (impl) impl->release();
  impl = nullptr;
}


void RowIndex::shrink(size_t nrows, size_t ncols) {
  xassert(impl && static_cast<size_t>(impl->refcount) >= ncols + 1);
  if (static_cast<size_t>(impl->refcount) == ncols + 1) {
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
  size_t szlen = length();
  xassert(target.size() >= szlen);
  switch (impl->type) {
    case RowIndexType::ARR32: {
      std::memcpy(target.data(), indices32(), szlen * sizeof(int32_t));
      break;
    }
    case RowIndexType::SLICE: {
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
  if (nrows < static_cast<size_t>(max())) {
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




//------------------------------------------------------------------------------
// RowIndexImpl
//------------------------------------------------------------------------------

RowIndexImpl::RowIndexImpl()
    : length(0),
      min(0),
      max(0),
      refcount(0),
      type(RowIndexType::UNKNOWN),
      ascending(false) {}

RowIndexImpl::~RowIndexImpl() {}


void RowIndexImpl::acquire() {
  refcount++;
}

void RowIndexImpl::release() {
  refcount--;
  if (!refcount) delete this;
}



void RowIndexImpl::verify_integrity() const {
  if (refcount == 0) {
    throw AssertionError() << "RowIndex has refcount of 0";
  }
  if (length == 0 && (min || max)) {
    throw AssertionError() << "RowIndex has length 0, but either min = " << min
        << " or max = " << max << " are non-zero";
  }
  if (min > RowIndex::MAX) {
    int64_t imin = static_cast<int64_t>(min);
    throw AssertionError() << "min value in RowIndex is negative: " << imin;
  }
  if (max > RowIndex::MAX) {
    int64_t imax = static_cast<int64_t>(max);
    throw AssertionError() << "max value in RowIndex is negative: " << imax;
  }
  if (min > max) {
    throw AssertionError() << "min value in RowIndex is larger than max: min = "
        << min << ", max = " << max;
  }
}
