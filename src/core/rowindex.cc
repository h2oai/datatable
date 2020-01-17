//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "utils/array.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "parallel/api.h"     // dt::parallel_for_static
#include "datatablemodule.h"
#include "rowindex.h"
#include "rowindex_impl.h"


constexpr int32_t RowIndex::NA_ARR32;
constexpr int64_t RowIndex::NA_ARR64;


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

RowIndex::RowIndex() {
  impl = nullptr;
}

// copy-constructor, performs shallow copying
RowIndex::RowIndex(const RowIndex& other) : RowIndex() {
  if (other.impl) impl = other.impl->acquire();
}

// assignment operator, performs shallow copying
RowIndex& RowIndex::operator=(const RowIndex& other) {
  if (impl) impl = impl->release();
  if (other.impl) impl = other.impl->acquire();
  return *this;
}

// move-constructor
RowIndex::RowIndex(RowIndex&& other) {
  impl = other.impl;
  other.impl = nullptr;
}

RowIndex::~RowIndex() {
  if (impl) impl = impl->release();
}


// Private constructor
RowIndex::RowIndex(RowIndexImpl* rii) {
  impl = rii? rii->acquire() : nullptr;
}


RowIndex::RowIndex(size_t start, size_t count, size_t step) {
  impl = (new SliceRowIndexImpl(start, count, step))->acquire();
}

RowIndex::RowIndex(arr32_t&& arr, bool sorted) {
  impl = (new ArrayRowIndexImpl(std::move(arr), sorted))->acquire();
}

RowIndex::RowIndex(arr64_t&& arr, bool sorted) {
  impl = (new ArrayRowIndexImpl(std::move(arr), sorted))->acquire();
}

RowIndex::RowIndex(const Column& col) {
  impl = (new ArrayRowIndexImpl(col))->acquire();
}



template <typename T>
static RowIndex _concat(size_t n, const std::vector<RowIndex>& parts) {
  dt::array<T> data(n);
  size_t offset = 0;
  for (const RowIndex& ri : parts) {
    dt::array<T> subdata(ri.size(), data.data() + offset, /*owned=*/false);
    ri.extract_into(subdata);
    offset += ri.size();
  }
  return RowIndex(std::move(data));
}


RowIndex RowIndex::concat(const std::vector<RowIndex>& parts) {
  size_t total_size = 0;
  for (const RowIndex& ri : parts) {
    total_size += ri.size();
  }
  if (total_size <= std::numeric_limits<int32_t>::max()) {
    return _concat<int32_t>(total_size, parts);
  } else {
    return _concat<int64_t>(total_size, parts);
  }
}




//------------------------------------------------------------------------------
// API
//------------------------------------------------------------------------------

RowIndexType RowIndex::type() const noexcept {
  return impl? impl->type : RowIndexType::UNKNOWN;
}

bool RowIndex::isabsent() const {
  return impl == nullptr;
}

bool RowIndex::isslice() const {
  return impl && impl->type == RowIndexType::SLICE;
}

bool RowIndex::isarr32() const {
  return impl && impl->type == RowIndexType::ARR32;
}

bool RowIndex::isarr64() const {
  return impl && impl->type == RowIndexType::ARR64;
}

size_t RowIndex::size() const {
  return impl? impl->length : 0;
}

size_t RowIndex::max() const {
  return impl? impl->max : 0;
}

bool RowIndex::is_all_missing() const {
  return impl && !impl->max_valid;
}



bool RowIndex::get_element(size_t i, size_t* out) const {
  if (!impl) {
    *out = i;
    return true;
  }
  xassert(i < impl->length);
  return impl->get_element(i, out);
}

const int32_t* RowIndex::indices32() const noexcept {
  auto a = dynamic_cast<ArrayRowIndexImpl*>(impl);
  return a? a->indices32() : nullptr;
}
const int64_t* RowIndex::indices64() const noexcept {
  auto a = dynamic_cast<ArrayRowIndexImpl*>(impl);
  return a? a->indices64() : nullptr;
}

size_t RowIndex::slice_start() const noexcept {
  return slice_rowindex_get_start(impl);
}
size_t RowIndex::slice_step() const noexcept {
  return slice_rowindex_get_step(impl);
}



template <typename T>
static void _extract_into(const RowIndex& ri, dt::array<T>& target) {
  if (!ri) return;
  size_t ri_size = ri.size();
  xassert(target.size() >= ri_size);
  switch (ri.type()) {
    case RowIndexType::ARR32: {
      const int32_t* ri_data = ri.indices32();
      if (sizeof(T) == 4) {
        std::memcpy(target.data(), ri_data, ri_size * sizeof(T));
      }
      else {
        dt::parallel_for_static(ri_size,
          [&](size_t i) {
            target[i] = static_cast<T>(ri_data[i]);
          });
      }
      break;
    }
    case RowIndexType::ARR64: {
      const int64_t* ri_data = ri.indices64();
      if (sizeof(T) == 8) {
        std::memcpy(target.data(), ri_data, ri_size * sizeof(T));
      }
      else {
        dt::parallel_for_static(ri_size,
          [&](size_t i) {
            target[i] = static_cast<T>(ri_data[i]);
          });
      }
      break;
    }
    case RowIndexType::SLICE: {
      size_t start = ri.slice_start();
      size_t step = ri.slice_step();
      dt::parallel_for_static(ri_size,
        [&](size_t i) {
          target[i] = static_cast<T>(start + i * step);
        });
      break;
    }
    default:
      break;
  }
}

void RowIndex::extract_into(arr32_t& target) const {
  _extract_into<int32_t>(*this, target);
}

void RowIndex::extract_into(arr64_t& target) const {
  _extract_into<int64_t>(*this, target);
}


RowIndex operator *(const RowIndex& ri1, const RowIndex& ri2) {
  if (ri1.isabsent()) return RowIndex(ri2);
  if (ri2.isabsent()) return RowIndex(ri1);
  return RowIndex(ri1.impl->uplift_from(ri2.impl));
}


Buffer RowIndex::as_boolean_mask(size_t nrows) const {
  Buffer res = Buffer::mem(nrows);
  int8_t* data = static_cast<int8_t*>(res.xptr());
  if (isabsent()) {
    // No RowIndex is equivalent to having RowIndex over all rows, i.e. we
    // should return an array of all 1's.
    std::memset(data, 1, nrows);
  } else {
    std::memset(data, 0, nrows);
    iterate(0, size(), 1,
      [&](size_t, size_t j, bool jvalid) {
        if (jvalid) data[j] = 1;
      });
  }
  return res;
}


Buffer RowIndex::as_integer_mask(size_t nrows) const {
  Buffer res = Buffer::mem(nrows * 4);
  int32_t* data = static_cast<int32_t*>(res.xptr());
  std::fill(data, data + nrows, RowIndex::NA_ARR32);
  iterate(0, size(), 1,
    [&](size_t i, size_t j, bool jvalid) {
      if (jvalid) data[j] = static_cast<int32_t>(i);
    });
  return res;
}


RowIndex RowIndex::negate(size_t nrows) const {
  if (isabsent()) {
    // No RowIndex is equivalent to having RowIndex over all rows. The inverse
    // of that is a 0-length RowIndex.
    return RowIndex(new SliceRowIndexImpl(0, 0, 0));
  }
  if (size() == 0) {
    // An inverse of a 0-length RowIndex is a RowIndex over all rows, which we
    // return as an empty RowIndex object.
    return RowIndex();
  }
  if (nrows <= max()) {
    throw ValueError() << "Invalid nrows=" << nrows << " for a RowIndex with "
                          "largest index " << max();
  }
  return RowIndex(impl->negate(nrows));
}


size_t RowIndex::memory_footprint() const noexcept {
  // If multiple columns share a rowindex, we don't want to account for it
  // multiple times. Instead, try to assign each instance an "equal share"
  // of this object's memory footprint.
  return sizeof(this) + (impl? impl->memory_footprint() / impl->refcount : 0);
}


void RowIndex::verify_integrity() const {
  if (impl) impl->verify_integrity();
}




//------------------------------------------------------------------------------
// RowIndexImpl
//------------------------------------------------------------------------------

RowIndexImpl::RowIndexImpl()
    : length(0),
      refcount(0),
      type(RowIndexType::UNKNOWN),
      ascending(false),
      max_valid(true) {}

RowIndexImpl::~RowIndexImpl() = default;


RowIndexImpl* RowIndexImpl::acquire() {
  refcount++;
  return this;
}

RowIndexImpl* RowIndexImpl::release() {
  refcount--;
  if (refcount) return this;
  delete this;
  return nullptr;
}


void RowIndexImpl::verify_integrity() const {
  XAssert(refcount > 0);
  XAssert(length == 0? !max_valid : true);
  XAssert(max_valid? max <= RowIndex::MAX : true);
}
