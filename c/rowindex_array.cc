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
#include <algorithm>           // std::min, std::swap, std::move
#include <cstdlib>             // std::memcpy
#include <limits>              // std::numeric_limits
#include "parallel/api.h"      // nested_for_static
#include "parallel/atomic.h"
#include "utils/exceptions.h"  // ValueError, RuntimeError
#include "utils/assert.h"
#include "column.h"            // Column
#include "rowindex_impl.h"

#ifndef NDEBUG
  inline static void test(ArrayRowIndexImpl* o) {
    o->refcount++;
    o->verify_integrity();
    o->refcount--;
  }
#else
  #define test(ptr)
#endif



//------------------------------------------------------------------------------
// ArrayRowIndexImpl implementation
//------------------------------------------------------------------------------

ArrayRowIndexImpl::ArrayRowIndexImpl(arr32_t&& array, bool sorted) {
  xassert(array.size() <= Column::MAX_ARR32_SIZE);
  type = RowIndexType::ARR32;
  ascending = sorted;
  length = array.size();
  buf_ = array.to_memoryrange();
  set_min_max();
  test(this);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr64_t&& array, bool sorted) {
  type = RowIndexType::ARR64;
  ascending = sorted;
  length = array.size();
  buf_ = array.to_memoryrange();
  set_min_max();
  test(this);
}


// Construct from a list of slices
ArrayRowIndexImpl::ArrayRowIndexImpl(
    const arr64_t& starts, const arr64_t& counts, const arr64_t& steps)
{
  size_t n = starts.size();
  xassert(n == counts.size() && n == steps.size());
  ascending = true;

  // Compute the total number of elements, and the largest index that needs
  // to be stored. Also check for potential overflows / invalid values.
  length = 0;
  min = std::numeric_limits<size_t>::max();
  max = 0;
  for (size_t i = 0; i < n; ++i) {
    size_t start = static_cast<size_t>(starts[i]);
    size_t step  = static_cast<size_t>(steps[i]);
    size_t len   = static_cast<size_t>(counts[i]);
    if (start == RowIndex::NA && step == 0 && len <= RowIndex::MAX) {}
    else {
      SliceRowIndexImpl tmp(start, len, step);  // check triple's validity
      if (!tmp.ascending || tmp.min < max) ascending = false;
      if (tmp.min < min) min = tmp.min;
      if (tmp.max > max) max = tmp.max;
    }
    length += len;
  }
  if (min > max) {
    min = max = RowIndex::NA;
  }
  xassert(min >= 0 && min <= max);

  if (length <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::ARR32;
    _resize_data();
    auto rowsptr = static_cast<int32_t*>(buf_.xptr());
    for (size_t i = 0; i < n; ++i) {
      int32_t j = static_cast<int32_t>(starts[i]);
      int32_t icount = static_cast<int32_t>(counts[i]);
      int32_t istep = static_cast<int32_t>(steps[i]);
      for (int32_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == static_cast<const int32_t*>(buf_.rptr()) + length);
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    auto rowsptr = static_cast<int64_t*>(buf_.xptr());
    for (size_t i = 0; i < n; ++i) {
      int64_t j = starts[i];
      int64_t icount = counts[i];
      int64_t istep = steps[i];
      for (int64_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == static_cast<const int64_t*>(buf_.rptr()) + length);
  }
  test(this);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(const Column& col) {
  ascending = false;
  switch (col.stype()) {
    case SType::BOOL:
      init_from_boolean_column(col);
      break;
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:
      init_from_integer_column(col);
      break;
    default:
      throw ValueError() << "Column is not of boolean or integer type";
  }
  test(this);
}




void ArrayRowIndexImpl::set_min_max() {
  if (type == RowIndexType::ARR32) {
    _set_min_max<int32_t>();
  } else {
    _set_min_max<int64_t>();
  }
}

template <typename T>
void ArrayRowIndexImpl::_set_min_max() {
  constexpr T TMAX = std::numeric_limits<T>::max();
  const T* idata = static_cast<const T*>(buf_.rptr());
  if (length == 1) ascending = true;
  if (length == 0) {
    min = max = RowIndex::NA;
  }
  else if (ascending) {
    for (size_t j = 0; j < length; ++j) {
      min = static_cast<size_t>(idata[j]);
      if (min != RowIndex::NA) break;
    }
    if (min == RowIndex::NA) {
      max = min;
    } else {
      for (size_t j = length - 1; j < length; --j) {
        max = static_cast<size_t>(idata[j]);
        if (max != RowIndex::NA) break;
      }
    }
  }
  else {
    std::atomic<T> amin { TMAX };
    std::atomic<T> amax { -TMAX };
    dt::parallel_region(
      [&] {
        T local_min = TMAX;
        T local_max = -TMAX;
        dt::nested_for_static(length,
          [&](size_t i) {
            T t = idata[i];
            if (t == -1) return;
            if (t < local_min) local_min = t;
            if (t > local_max) local_max = t;
          });
        dt::atomic_fetch_min(&amin, local_min);
        dt::atomic_fetch_max(&amax, local_max);
      });
    T tmin = amin.load();
    T tmax = amax.load();
    if (tmin == TMAX && tmax == -TMAX) tmin = tmax = -1;
    min = static_cast<size_t>(tmin);
    max = static_cast<size_t>(tmax);
  }
  xassert(max >= min);
  xassert(max == RowIndex::NA || max <= RowIndex::MAX);
  xassert(min == RowIndex::NA || min <= RowIndex::MAX);
}


void ArrayRowIndexImpl::init_from_boolean_column(const Column& col) {
  xassert(col.stype() == SType::BOOL);
  // total # of 1s in the column
  length = static_cast<size_t>(col.stats()->sum());

  if (length == 0) {
    // no need to do anything: the data arrays already have 0 length
    type = RowIndexType::ARR32;
    return;
  }
  int32_t value;
  if (length <= INT32_MAX && col.nrows() <= INT32_MAX) {
    type = RowIndexType::ARR32;
    _resize_data();
    auto ind32 = static_cast<int32_t*>(buf_.xptr());
    size_t k = 0;
    for (size_t i = 0; i < col.nrows(); ++i) {
      bool isvalid = col.get_element(i, &value);
      if (value && isvalid) {
        ind32[k++] = static_cast<int32_t>(i);
      }
    }
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    auto ind64 = static_cast<int64_t*>(buf_.xptr());
    size_t k = 0;
    for (size_t i = 0; i < col.nrows(); ++i) {
      bool isvalid = col.get_element(i, &value);
      if (value && isvalid) {
        ind64[k++] = static_cast<int64_t>(i);
      }
    }
  }
  ascending = true;
  set_min_max();
}


void ArrayRowIndexImpl::init_from_integer_column(const Column& col) {
  if (col.nrows() == 0) {
    min = max = RowIndex::NA;
  } else {
    int64_t imin = col.stats()->min_int();
    int64_t imax = col.stats()->max_int();
    if (imin < -1) {
      throw ValueError() << "Row indices in integer column cannot be negative";
    }
    if (col.na_count()) {
      throw ValueError() << "RowIndex source column contains NA values.";
    }
    if (imin == -1) imin = 0;
    min = static_cast<size_t>(imin);
    max = static_cast<size_t>(imax);
  }
  length = col.nrows();
  bool allow_arr32 = (length <= Column::MAX_ARR32_SIZE) &&
                     (max <= Column::MAX_ARR32_SIZE);

  if (!col.is_virtual()) {
    if (col.stype() == SType::INT32 && allow_arr32) {
      type = RowIndexType::ARR32;
      buf_ = col.get_data_buffer();
      return;
    }
    if (col.stype() == SType::INT64) {
      type = RowIndexType::ARR64;
      buf_ = col.get_data_buffer();
      return;
    }
  }

  if (allow_arr32) {
    type = RowIndexType::ARR32;
    _resize_data();
    // Column cast either converts the data, or memcpy-es it. The `col3`s data
    // will be written into `xbuf`, which is just a view onto `ind32`. Also,
    // since `xbuf` is ExternalMemBuf, its memory won't be reclaimed when
    // the column is destructed.
    Buffer xbuf = Buffer::view(buf_, buf_.size(), 0);
    xassert(xbuf.is_writable());
    auto col3 = col.cast(SType::INT32, std::move(xbuf));
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    Buffer xbuf = Buffer::view(buf_, buf_.size(), 0);
    xassert(xbuf.is_writable());
    auto col3 = col.cast(SType::INT64, std::move(xbuf));
  }
}


const int32_t* ArrayRowIndexImpl::indices32() const noexcept {
  return static_cast<const int32_t*>(buf_.rptr());
}
const int64_t* ArrayRowIndexImpl::indices64() const noexcept {
  return static_cast<const int64_t*>(buf_.rptr());
}



RowIndexImpl* ArrayRowIndexImpl::uplift_from(const RowIndexImpl* rii) const {
  RowIndexType uptype = rii->type;
  if (uptype == RowIndexType::SLICE) {
    size_t start = slice_rowindex_get_start(rii);
    size_t step  = slice_rowindex_get_step(rii);
    arr64_t rowsres(length);
    if (type == RowIndexType::ARR32) {
      auto ind32 = indices32();
      for (size_t i = 0; i < length; ++i) {
        size_t j = start + static_cast<size_t>(ind32[i]) * step;
        rowsres[i] = static_cast<int64_t>(j);
      }
    } else {
      auto ind64 = indices64();
      for (size_t i = 0; i < length; ++i) {
        size_t j = start + static_cast<size_t>(ind64[i]) * step;
        rowsres[i] = static_cast<int64_t>(j);
      }
    }
    bool res_sorted = ascending && slice_rowindex_increasing(rii);
    auto res = new ArrayRowIndexImpl(std::move(rowsres), res_sorted);
    res->compactify();
    return res;
  }
  xassert(max < rii->length || max == RowIndex::NA);
  if (uptype == RowIndexType::ARR32 && type == RowIndexType::ARR32) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    arr32_t rowsres(length);
    auto rows_ab = arii->indices32();
    auto rows_bc = indices32();
    for (size_t i = 0; i < length; ++i) {
      rowsres[i] = rows_ab[rows_bc[i]];
    }
    bool res_sorted = ascending && arii->ascending;
    return new ArrayRowIndexImpl(std::move(rowsres), res_sorted);
  }
  if (uptype == RowIndexType::ARR32 || uptype == RowIndexType::ARR64) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    arr64_t rowsres(length);
    if (uptype == RowIndexType::ARR32 && type == RowIndexType::ARR64) {
      auto rows_ab = arii->indices32();
      auto rows_bc = indices64();
      for (size_t i = 0; i < length; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR32) {
      auto rows_ab = arii->indices64();
      auto rows_bc = indices32();
      for (size_t i = 0; i < length; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR64) {
      auto rows_ab = arii->indices64();
      auto rows_bc = indices64();
      for (size_t i = 0; i < length; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    bool res_sorted = ascending && arii->ascending;
    auto res = new ArrayRowIndexImpl(std::move(rowsres), res_sorted);
    res->compactify();
    return res;
  }
  throw RuntimeError() << "Unknown RowIndexType " << static_cast<int>(uptype);
}


/**
 * Attempt to convert an ARR64 RowIndex object into the ARR32 format. If such
 * conversion is possible, the object will be modified in-place (regardless of
 * its refcount).
 */
void ArrayRowIndexImpl::compactify()
{
  if (type == RowIndexType::ARR32) return;
  if ((max > INT32_MAX && max != RowIndex::MAX) || length > INT32_MAX) return;

  int32_t* ind32 = static_cast<int32_t*>(buf_.xptr());
  int64_t* ind64 = reinterpret_cast<int64_t*>(ind32);
  for (size_t i = 0; i < length; ++i) {
    ind32[i] = static_cast<int32_t>(ind64[i]);
  }
  type = RowIndexType::ARR32;
  _resize_data();
}


template <typename TI, typename TO>
RowIndexImpl* ArrayRowIndexImpl::negate_impl(size_t nrows) const
{
  auto inputs = static_cast<const TI*>(buf_.rptr());
  size_t newsize = nrows - length;
  size_t inpsize = length;
  dt::array<TO> outputs(newsize);
  TO orows = static_cast<TO>(nrows);

  TO next_index_to_skip = static_cast<TO>(inputs[0]);
  size_t j = 1;  // next index to read from the `inputs` array
  size_t k = 0;  // next index to write into the `outputs` array
  for (TO i = 0; i < orows; ++i) {
    if (i == next_index_to_skip) {
      next_index_to_skip =
        j < inpsize? static_cast<TO>(inputs[j++]) : orows;
      if (next_index_to_skip <= i) {
        throw ValueError() << "Cannot invert RowIndex which is not sorted";
      }
    } else {
      outputs[k++] = i;
    }
  }

  return new ArrayRowIndexImpl(std::move(outputs), true);
}


RowIndexImpl* ArrayRowIndexImpl::negate(size_t nrows) const {
  xassert(nrows >= length);
  if (type == RowIndexType::ARR32) {
    if (nrows <= INT32_MAX)
      return negate_impl<int32_t, int32_t>(nrows);
    else
      return negate_impl<int32_t, int64_t>(nrows);
  } else {
    if (nrows <= INT32_MAX)
      return negate_impl<int64_t, int32_t>(nrows);
    else
      return negate_impl<int64_t, int64_t>(nrows);
  }
}



size_t ArrayRowIndexImpl::nth(size_t i) const {
  if (type == RowIndexType::ARR32)
    return static_cast<size_t>(indices32()[i]);
  else
    return static_cast<size_t>(indices64()[i]);
}



size_t ArrayRowIndexImpl::memory_footprint() const noexcept {
  return sizeof(*this) + buf_.memory_footprint();
}


template <typename T>
static void verify_integrity_helper(
    const void* data, size_t len, size_t min, size_t max, bool sorted)
{
  constexpr T TMAX = std::numeric_limits<T>::max();
  auto ind = static_cast<const T*>(data);
  T tmin = TMAX;
  T tmax = -TMAX;
  bool check_sorted = sorted;
  for (size_t i = 0; i < len; ++i) {
    T x = ind[i];
    if (x == -1) continue;
    if (x < 0) {
      throw AssertionError()
          << "Element " << i << " in the ArrayRowIndex is negative: " << x;
    }
    if (x < tmin) tmin = x;
    if (x > tmax) tmax = x;
    if (check_sorted && i > 0 && x < ind[i-1]) check_sorted = false;
  }
  if (tmin == TMAX && tmax == -TMAX) tmin = tmax = -1;
  if (check_sorted != sorted) {
    throw AssertionError()
        << "ArrrayRowIndex is marked as sorted, but actually it isn't.";
  }
  if (!((static_cast<size_t>(tmin) == min || min == 0) &&
        static_cast<size_t>(tmax) == max)) {
    throw AssertionError()
        << "Mismatching min/max values in the ArrayRowIndex min=" << min
        << "/max=" << max << " compared to the computed min=" << tmin
        << "/max=" << tmax;
  }
}

void ArrayRowIndexImpl::verify_integrity() const {
  RowIndexImpl::verify_integrity();
  buf_.verify_integrity();

  if (type == RowIndexType::ARR32) {
    verify_integrity_helper<int32_t>(buf_.rptr(), length, min, max, ascending);
  } else if (type == RowIndexType::ARR64) {
    verify_integrity_helper<int64_t>(buf_.rptr(), length, min, max, ascending);
  } else {
    throw AssertionError() << "Invalid type = " << static_cast<int>(type)
        << " in ArrayRowIndex";
  }
}


void ArrayRowIndexImpl::_resize_data() {
  size_t elemsize = type == RowIndexType::ARR32? 4 : 8;
  buf_.resize(length * elemsize);
}
