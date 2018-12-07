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
#include <algorithm>           // std::min, std::swap, std::move
#include <cstdlib>             // std::memcpy
#include <limits>              // std::numeric_limits
#include "column.h"            // Column, BoolColumn
#include "memrange.h"          // MemoryRange
#include "rowindex.h"
#include "rowindex_impl.h"
#include "utils/exceptions.h"  // ValueError, RuntimeError
#include "utils/assert.h"
#include "utils/parallel.h"



//------------------------------------------------------------------------------
// ArrayRowIndexImpl implementation
//------------------------------------------------------------------------------

ArrayRowIndexImpl::ArrayRowIndexImpl(arr32_t&& array, bool sorted) {
  type = RowIndexType::ARR32;
  ascending = sorted;
  length = array.size();
  xassert(length <= std::numeric_limits<int32_t>::max());
  owned = array.data_owned();
  data = array.release();
  set_min_max();
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr64_t&& array, bool sorted) {
  type = RowIndexType::ARR64;
  ascending = sorted;
  length = array.size();
  owned = array.data_owned();
  data = array.release();
  set_min_max();
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr32_t&& array, size_t _min, size_t _max)
{
  type = RowIndexType::ARR32;
  ascending = false;
  length = array.size();
  xassert(length <= std::numeric_limits<int32_t>::max());
  owned = array.data_owned();
  data = array.release();
  min = _min;
  max = _max;
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr64_t&& array, size_t _min, size_t _max)
{
  type = RowIndexType::ARR64;
  ascending = false;
  length = array.size();
  owned = array.data_owned();
  data = array.release();
  min = _min;
  max = _max;
}


// Construct from a list of slices
ArrayRowIndexImpl::ArrayRowIndexImpl(
    const arr64_t& starts, const arr64_t& counts, const arr64_t& steps)
{
  size_t n = starts.size();
  xassert(n == counts.size() && n == steps.size());
  ascending = true;
  data = nullptr;
  owned = true;

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
    auto rowsptr = static_cast<int32_t*>(data);
    for (size_t i = 0; i < n; ++i) {
      int32_t j = static_cast<int32_t>(starts[i]);
      int32_t icount = static_cast<int32_t>(counts[i]);
      int32_t istep = static_cast<int32_t>(steps[i]);
      for (int32_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == static_cast<int32_t*>(data) + length);
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    auto rowsptr = static_cast<int64_t*>(data);
    for (size_t i = 0; i < n; ++i) {
      int64_t j = starts[i];
      int64_t icount = counts[i];
      int64_t istep = steps[i];
      for (int64_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == static_cast<int64_t*>(data) + length);
  }
}


ArrayRowIndexImpl::ArrayRowIndexImpl(const Column* col) {
  data = nullptr;
  owned = true;
  ascending = false;
  switch (col->stype()) {
    case SType::BOOL:
      init_from_boolean_column(static_cast<const BoolColumn*>(col));
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
}


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn32* ff, size_t n, bool sorted) {
  xassert(n <= std::numeric_limits<int32_t>::max());
  ascending = sorted;
  data = nullptr;
  owned = true;

  // Output buffer, where we will write the indices of selected rows. This
  // buffer is preallocated to the length of the original dataset, and it will
  // be re-alloced to the proper length in the end. The reason we don't want
  // to scale this array dynamically is because it reduces performance (at
  // least some of the reallocs will have to memmove the data, and moreover
  // the realloc has to occur within a critical section, slowing down the
  // team of threads).
  type = RowIndexType::ARR32;
  length = n;
  _resize_data();

  // Number of elements that were written (or tentatively written) so far
  // into the array `out`.
  size_t out_length = 0;
  // We divide the range of rows [0:n] into `num_chunks` pieces, each
  // (except the very last one) having `rows_per_chunk` rows. Each such piece
  // is a fundamental unit of work for this function: every thread in the team
  // works on a single chunk at a time, and then moves on to the next chunk
  // in the queue.
  size_t rows_per_chunk = 65536;
  size_t num_chunks = (n + rows_per_chunk - 1) / rows_per_chunk;
  int32_t* out_ptr = static_cast<int32_t*>(data);

  #pragma omp parallel
  {
    // Intermediate buffer where each thread stores the row numbers it found
    // before they are consolidated into the final output buffer.
    arr32_t buf(rows_per_chunk);

    // Number of elements that are currently being held in `buf`.
    size_t buf_length = 0;
    // Offset (within the output buffer) where this thread needs to save the
    // contents of its temporary buffer `buf`.
    // The algorithm works as follows: first, the thread calls `filterfn` to
    // fill up its buffer `buf`. After `filterfn` finishes, the variable
    // `buf_length` will contain the number of rows that were selected from
    // the current (`i`th) chunk. Those row numbers are stored in `buf`.
    // Then the thread enters the "ordered" section, where it stores the
    // current length of the output buffer into the `out_offset` variable,
    // and increases the `out_offset` as if it already copied the result
    // there. However the actual copying is done outside the "ordered"
    // section so as to block all other threads as little as possible.
    size_t out_offset = 0;

    #pragma omp for ordered schedule(dynamic, 1)
    for (size_t i = 0; i < num_chunks; ++i) {
      if (buf_length) {
        // This clause is conceptually located after the "ordered"
        // section -- however due to a bug in libgOMP the "ordered"
        // section must come last in the loop. So in order to circumvent
        // the bug, this block had to be moved to the front of the loop.
        size_t bufsize = buf_length * sizeof(int32_t);
        std::memcpy(out_ptr + out_offset, buf.data(), bufsize);
        buf_length = 0;
      }

      size_t row0 = i * rows_per_chunk;
      size_t row1 = std::min(row0 + rows_per_chunk, n);
      (*ff)(row0, row1, buf.data(), &buf_length);

      #pragma omp ordered
      {
        out_offset = out_length;
        out_length += buf_length;
      }
    }
    // Note: if the underlying array is small, then some threads may have
    // done nothing at all, and their buffers would be empty.
    if (buf_length) {
      size_t bufsize = buf_length * sizeof(int32_t);
      std::memcpy(out_ptr + out_offset, buf.data(), bufsize);
      buf_length = 0;
    }
  }

  // In the end we shrink the output buffer to the size corresponding to the
  // actual number of elements written.
  length = out_length;
  _resize_data();
  set_min_max();
}


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn64* ff, size_t n, bool sorted) {
  ascending = sorted;
  data = nullptr;
  owned = true;
  type = RowIndexType::ARR64;
  length = n;
  _resize_data();
  auto out_ptr = static_cast<int64_t*>(data);
  size_t out_length = 0;
  size_t rows_per_chunk = 65536;
  size_t num_chunks = (n + rows_per_chunk - 1) / rows_per_chunk;

  #pragma omp parallel
  {
    arr64_t buf(rows_per_chunk);
    size_t buf_length = 0;
    size_t out_offset = 0;

    #pragma omp for ordered schedule(dynamic, 1)
    for (size_t i = 0; i < num_chunks; ++i) {
      if (buf_length) {
        size_t bufsize = buf_length * sizeof(int64_t);
        std::memcpy(out_ptr + out_offset, buf.data(), bufsize);
        buf_length = 0;
      }
      size_t row0 = i * rows_per_chunk;
      size_t row1 = std::min(row0 + rows_per_chunk, n);
      (*ff)(row0, row1, buf.data(), &buf_length);
      #pragma omp ordered
      {
        out_offset = out_length;
        out_length += buf_length;
      }
    }
    if (buf_length) {
      size_t bufsize = buf_length * sizeof(int64_t);
      memcpy(out_ptr + out_offset, buf.data(), bufsize);
      buf_length = 0;
    }
  }
  length = out_length;
  _resize_data();
  set_min_max();
}


ArrayRowIndexImpl::~ArrayRowIndexImpl() {
  if (data && owned) {
    std::free(data);
  }
  data = nullptr;
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
  const T* idata = static_cast<const T*>(data);
  if (length == 1) ascending = true;
  if (length == 0) {
    min = max = RowIndex::NA;
  } else if (length == 1) {
    min = static_cast<size_t>(idata[0]);
    max = static_cast<size_t>(idata[length - 1]);
    if (min == RowIndex::NA || max == RowIndex::NA) {
      if (min == RowIndex::NA && max == RowIndex::NA) {
        min = max = 0;
      } else if (min == RowIndex::NA) {
        size_t j = 1;
        while (j < length && idata[j] == -1) ++j;
        min = static_cast<size_t>(idata[j]);
      } else {
        size_t j = length - 2;
        while (j < length && idata[j] == -1) --j;
        max = static_cast<size_t>(idata[j]);
      }
    }
    if (min > max) std::swap(min, max);
  } else {
    T tmin = std::numeric_limits<T>::max();
    T tmax = -std::numeric_limits<T>::max();
    #pragma omp parallel for schedule(static) \
        reduction(min:tmin) reduction(max:tmax)
    for (size_t j = 0; j < length; ++j) {
      T t = idata[j];
      if (t == -1) continue;
      if (t < tmin) tmin = t;
      if (t > tmax) tmax = t;
    }
    if (tmin == std::numeric_limits<T>::max() &&
        tmax == -std::numeric_limits<T>::max()) tmin = tmax = -1;
    min = static_cast<size_t>(tmin);
    max = static_cast<size_t>(tmax);
  }
  xassert(max >= min && (max == RowIndex::NA || max <= RowIndex::MAX)
                     && (min == RowIndex::NA || min <= RowIndex::MAX));
}


void ArrayRowIndexImpl::init_from_boolean_column(const BoolColumn* col) {
  const int8_t* tdata = col->elements_r();
  length = static_cast<size_t>(col->sum());  // total # of 1s in the column

  if (length == 0) {
    // no need to do anything: the data arrays already have 0 length
    type = RowIndexType::ARR32;
    return;
  }
  if (length <= INT32_MAX && col->nrows <= INT32_MAX) {
    type = RowIndexType::ARR32;
    _resize_data();
    auto ind32 = static_cast<int32_t*>(data);
    size_t k = 0;
    col->rowindex().iterate(0, col->nrows, 1,
      [&](size_t, size_t j) {
        if (tdata[j] == 1)
          ind32[k++] = static_cast<int32_t>(j);
      });
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    auto ind64 = static_cast<int64_t*>(data);
    size_t k = 0;
    col->rowindex().iterate(0, col->nrows, 1,
      [&](size_t, size_t j) {
        if (tdata[j] == 1)
          ind64[k++] = static_cast<int64_t>(j);
      });
  }
  ascending = true;
  set_min_max();
}


void ArrayRowIndexImpl::init_from_integer_column(const Column* col) {
  if (col->countna()) {
    throw ValueError() << "RowIndex source column contains NA values.";
  }
  int64_t imin = col->min_int64();
  int64_t imax = col->max_int64();
  if (imin < 0) {
    throw ValueError() << "Row indices in integer column cannot be negative";
  }
  min = static_cast<size_t>(imin);
  max = static_cast<size_t>(imax);
  Column* col2 = col->shallowcopy();
  col2->reify();  // noop if col has no rowindex

  length = col->nrows;
  Column* col3 = nullptr;
  if (length <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::ARR32;
    _resize_data();
    // Column cast either converts the data, or memcpy-es it. The `col3`s data
    // will be written into `xbuf`, which is just a view onto `ind32`. Also,
    // since `xbuf` is ExternalMemBuf, its memory won't be reclaimed when
    // the column is destructed.
    MemoryRange xbuf = MemoryRange::external(data, length * sizeof(int32_t));
    xassert(xbuf.is_writable());
    col3 = col2->cast(SType::INT32, std::move(xbuf));
  } else {
    type = RowIndexType::ARR64;
    _resize_data();
    MemoryRange xbuf = MemoryRange::external(data, length * sizeof(int64_t));
    xassert(xbuf.is_writable());
    col3 = col2->cast(SType::INT64, std::move(xbuf));
  }

  delete col2;
  delete col3;
}


const int32_t* ArrayRowIndexImpl::indices32() const noexcept {
  return static_cast<int32_t*>(data);
}
const int64_t* ArrayRowIndexImpl::indices64() const noexcept {
  return static_cast<int64_t*>(data);
}



RowIndexImpl* ArrayRowIndexImpl::uplift_from(const RowIndexImpl* rii) {
  RowIndexType uptype = rii->type;
  if (uptype == RowIndexType::SLICE) {
    size_t start = slice_rowindex_get_start(rii);
    size_t step  = slice_rowindex_get_step(rii);
    arr64_t rowsres(length);
    if (type == RowIndexType::ARR32) {
      auto ind32 = static_cast<const int32_t*>(data);
      for (size_t i = 0; i < length; ++i) {
        size_t j = start + static_cast<size_t>(ind32[i]) * step;
        rowsres[i] = static_cast<int64_t>(j);
      }
    } else {
      auto ind64 = static_cast<const int64_t*>(data);
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
  if (uptype == RowIndexType::ARR32 && type == RowIndexType::ARR32) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    arr32_t rowsres(length);
    auto rows_ab = static_cast<const int32_t*>(arii->data);
    auto rows_bc = static_cast<const int32_t*>(data);
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
      auto rows_ab = static_cast<const int32_t*>(arii->data);
      auto rows_bc = static_cast<const int64_t*>(data);
      for (size_t i = 0; i < length; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR32) {
      auto rows_ab = static_cast<const int64_t*>(arii->data);
      auto rows_bc = static_cast<const int32_t*>(data);
      for (size_t i = 0; i < length; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR64) {
      auto rows_ab = static_cast<const int64_t*>(arii->data);
      auto rows_bc = static_cast<const int64_t*>(data);
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

  int32_t* ind32 = static_cast<int32_t*>(data);
  int64_t* ind64 = static_cast<int64_t*>(data);
  for (size_t i = 0; i < length; ++i) {
    ind32[i] = static_cast<int32_t>(ind64[i]);
  }
  type = RowIndexType::ARR32;
  _resize_data();
}


template <typename TI, typename TO>
RowIndexImpl* ArrayRowIndexImpl::inverse_impl(size_t nrows) const
{
  auto inputs = static_cast<const TI*>(data);
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


RowIndexImpl* ArrayRowIndexImpl::inverse(size_t nrows) const {
  xassert(nrows >= length);
  if (type == RowIndexType::ARR32) {
    if (nrows <= INT32_MAX)
      return inverse_impl<int32_t, int32_t>(nrows);
    else
      return inverse_impl<int32_t, int64_t>(nrows);
  } else {
    if (nrows <= INT32_MAX)
      return inverse_impl<int64_t, int32_t>(nrows);
    else
      return inverse_impl<int64_t, int64_t>(nrows);
  }
}


void ArrayRowIndexImpl::shrink(size_t n) {
  xassert(n < length);
  length = n;
  _resize_data();
  set_min_max();
}

RowIndexImpl* ArrayRowIndexImpl::shrunk(size_t n) {
  xassert(n < length);
  if (type == RowIndexType::ARR32) {
    arr32_t new_ind32(n);
    std::memcpy(new_ind32.data(), data, n * sizeof(int32_t));
    return new ArrayRowIndexImpl(std::move(new_ind32), ascending);
  } else {
    arr64_t new_ind64(n);
    std::memcpy(new_ind64.data(), data, n * sizeof(int64_t));
    return new ArrayRowIndexImpl(std::move(new_ind64), ascending);
  }
}


void ArrayRowIndexImpl::resize(size_t n) {
  size_t oldlen = length;
  length = n;
  _resize_data();
  if (n <= oldlen) {
    set_min_max();
  } else {
    size_t elemsize = (type == RowIndexType::ARR32)? 4 : 8;
    std::memset(static_cast<char*>(data) + oldlen * elemsize,
                -1, elemsize * (n - oldlen));
  }
}

RowIndexImpl* ArrayRowIndexImpl::resized(size_t n) {
  size_t ncopy = std::min(n, length);
  if (type == RowIndexType::ARR32) {
    arr32_t new_ind32(n);
    std::memcpy(new_ind32.data(), data, ncopy * 4);
    std::memset(new_ind32.data() + ncopy, -1, (n - ncopy) * 4);
    return new ArrayRowIndexImpl(std::move(new_ind32), ascending);
  } else {
    arr64_t new_ind64(n);
    std::memcpy(new_ind64.data(), data, n * 8);
    std::memset(new_ind64.data() + ncopy, -1, (n - ncopy) * 8);
    return new ArrayRowIndexImpl(std::move(new_ind64), ascending);
  }
}



size_t ArrayRowIndexImpl::nth(size_t i) const {
  if (type == RowIndexType::ARR32)
    return static_cast<size_t>(static_cast<int32_t*>(data)[i]);
  else
    return static_cast<size_t>(static_cast<int64_t*>(data)[i]);
}



size_t ArrayRowIndexImpl::memory_footprint() const {
  return sizeof(*this) + length * (type == RowIndexType::ARR32? 4 : 8);
}


template <typename T>
static void verify_integrity_helper(
    void* data, size_t len, size_t min, size_t max, bool sorted)
{
  auto ind = static_cast<const T*>(data);
  T tmin = std::numeric_limits<T>::max();
  T tmax = 0;
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
  if (!len) tmin = tmax = -1;
  if (static_cast<size_t>(tmin) != min || static_cast<size_t>(tmax) != max) {
    throw AssertionError()
        << "Mismatching min/max values in the ArrayRowIndex min=" << min
        << "/max=" << max << " compared to the computed min=" << tmin
        << "/max=" << tmax;
  }
  if (check_sorted != sorted) {
    throw AssertionError()
        << "ArrrayRowIndex is marked as sorted, but actually it isn't.";
  }
}

void ArrayRowIndexImpl::verify_integrity() const {
  RowIndexImpl::verify_integrity();

  if (type == RowIndexType::ARR32) {
    verify_integrity_helper<int32_t>(data, length, min, max, ascending);
  } else if (type == RowIndexType::ARR64) {
    verify_integrity_helper<int64_t>(data, length, min, max, ascending);
  } else {
    throw AssertionError() << "Invalid type = " << static_cast<int>(type)
        << " in ArrayRowIndex";
  }
}


void ArrayRowIndexImpl::_resize_data() {
  if (!owned) {
    throw ValueError() << "Cannot resize data in RowIndex: not owned";
  }
  size_t elemsize = type == RowIndexType::ARR32? 4 : 8;
  size_t allocsize = length * elemsize;
  if (allocsize) {
    void* ptr = std::realloc(data, allocsize);
    if (!ptr) {
      throw MemoryError() << "Cannot allocate " << allocsize << " bytes "
          "for a RowIndex object";
    }
    data = ptr;
  } else {
    // If allocsize==0, the behavior of std::realloc is implementation-defined
    // See https://en.cppreference.com/w/cpp/memory/c/realloc
    std::free(data);
    data = nullptr;
  }
}
