//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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

ArrayRowIndexImpl::ArrayRowIndexImpl(arr32_t&& array, bool sorted)
    : ind32(std::move(array)), ind64()
{
  is_sorted = sorted;
  type = RowIndexType::ARR32;
  length = ind32.size();
  xassert(length <= std::numeric_limits<int32_t>::max());
  set_min_max<int32_t>(ind32);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr64_t&& array, bool sorted)
    : ind32(), ind64(std::move(array))
{
  is_sorted = sorted;
  type = RowIndexType::ARR64;
  length = ind64.size();
  set_min_max<int64_t>(ind64);
}


// Construct from a list of slices
ArrayRowIndexImpl::ArrayRowIndexImpl(
    const arr64_t& starts, const arr64_t& counts, const arr64_t& steps)
{
  size_t n = starts.size();
  xassert(n == counts.size() && n == steps.size());
  is_sorted = false;

  // Compute the total number of elements, and the largest index that needs
  // to be stored. Also check for potential overflows / invalid values.
  length = 0;
  min = INT64_MAX;
  max = 0;
  for (size_t i = 0; i < n; ++i) {
    int64_t start = starts[i];
    int64_t step = steps[i];
    int64_t len = counts[i];
    SliceRowIndexImpl tmp(start, len, step);  // check triple's validity
    if (len == 0) continue;
    int64_t end = start + step * (len - 1);
    if (start < min) min = start;
    if (start > max) max = start;
    if (end < min) min = end;
    if (end > max) max = end;
    length += static_cast<size_t>(len);
  }
  if (max == 0) min = 0;
  xassert(min >= 0 && min <= max);
  size_t zlen = static_cast<size_t>(length);

  if (zlen <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::ARR32;
    ind32.resize(zlen);
    int32_t* rowsptr = ind32.data();
    for (size_t i = 0; i < n; ++i) {
      int32_t j = static_cast<int32_t>(starts[i]);
      int32_t icount = static_cast<int32_t>(counts[i]);
      int32_t istep = static_cast<int32_t>(steps[i]);
      for (int32_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == &ind32[zlen]);
  } else {
    type = RowIndexType::ARR64;
    ind64.resize(zlen);
    int64_t* rowsptr = ind64.data();
    for (size_t i = 0; i < n; ++i) {
      int64_t j = starts[i];
      int64_t icount = counts[i];
      int64_t istep = steps[i];
      for (int64_t k = 0; k < icount; ++k) {
        *rowsptr++ = j;
        j += istep;
      }
    }
    xassert(rowsptr == &ind64[zlen]);
  }
}


ArrayRowIndexImpl::ArrayRowIndexImpl(Column* col) {
  is_sorted = false;
  switch (col->stype()) {
    case SType::BOOL:
      init_from_boolean_column(static_cast<BoolColumn*>(col));
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


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn32* ff, int64_t n, bool sorted) {
  xassert(n <= std::numeric_limits<int32_t>::max());
  is_sorted = sorted;

  // Output buffer, where we will write the indices of selected rows. This
  // buffer is preallocated to the length of the original dataset, and it will
  // be re-alloced to the proper length in the end. The reason we don't want
  // to scale this array dynamically is because it reduces performance (at
  // least some of the reallocs will have to memmove the data, and moreover
  // the realloc has to occur within a critical section, slowing down the
  // team of threads).
  ind32.resize(static_cast<size_t>(n));

  // Number of elements that were written (or tentatively written) so far
  // into the array `out`.
  size_t out_length = 0;
  // We divide the range of rows [0:n] into `num_chunks` pieces, each
  // (except the very last one) having `rows_per_chunk` rows. Each such piece
  // is a fundamental unit of work for this function: every thread in the team
  // works on a single chunk at a time, and then moves on to the next chunk
  // in the queue.
  int64_t rows_per_chunk = 65536;
  int64_t num_chunks = (n + rows_per_chunk - 1) / rows_per_chunk;
  size_t zrows_per_chunk = static_cast<size_t>(rows_per_chunk);

  #pragma omp parallel
  {
    // Intermediate buffer where each thread stores the row numbers it found
    // before they are consolidated into the final output buffer.
    arr32_t buf(zrows_per_chunk);

    // Number of elements that are currently being held in `buf`.
    int32_t buf_length = 0;
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
    for (int64_t i = 0; i < num_chunks; ++i) {
      if (buf_length) {
        // This clause is conceptually located after the "ordered"
        // section -- however due to a bug in libgOMP the "ordered"
        // section must come last in the loop. So in order to circumvent
        // the bug, this block had to be moved to the front of the loop.
        size_t bufsize = static_cast<size_t>(buf_length) * sizeof(int32_t);
        std::memcpy(ind32.data() + out_offset, buf.data(), bufsize);
        buf_length = 0;
      }

      int64_t row0 = i * rows_per_chunk;
      int64_t row1 = std::min(row0 + rows_per_chunk, n);
      (*ff)(row0, row1, buf.data(), &buf_length);

      #pragma omp ordered
      {
        out_offset = out_length;
        out_length += static_cast<size_t>(buf_length);
      }
    }
    // Note: if the underlying array is small, then some threads may have
    // done nothing at all, and their buffers would be empty.
    if (buf_length) {
      size_t bufsize = static_cast<size_t>(buf_length) * sizeof(int32_t);
      std::memcpy(ind32.data() + out_offset, buf.data(), bufsize);
      buf_length = 0;
    }
  }

  // In the end we shrink the output buffer to the size corresponding to the
  // actual number of elements written.
  ind32.resize(out_length);
  length = out_length;
  type = RowIndexType::ARR32;
  set_min_max(ind32);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn64* ff, int64_t n, bool sorted) {
  is_sorted = sorted;
  size_t out_length = 0;
  int64_t rows_per_chunk = 65536;
  int64_t num_chunks = (n + rows_per_chunk - 1) / rows_per_chunk;
  size_t zrows_per_chunk = static_cast<size_t>(rows_per_chunk);

  ind64.resize(static_cast<size_t>(n));
  #pragma omp parallel
  {
    arr64_t buf(zrows_per_chunk);
    int32_t buf_length = 0;
    size_t out_offset = 0;

    #pragma omp for ordered schedule(dynamic, 1)
    for (int64_t i = 0; i < num_chunks; ++i) {
      if (buf_length) {
        size_t bufsize = static_cast<size_t>(buf_length) * sizeof(int64_t);
        std::memcpy(ind64.data() + out_offset, buf.data(), bufsize);
        buf_length = 0;
      }
      int64_t row0 = i * rows_per_chunk;
      int64_t row1 = std::min(row0 + rows_per_chunk, n);
      (*ff)(row0, row1, buf.data(), &buf_length);
      #pragma omp ordered
      {
        out_offset = out_length;
        out_length += static_cast<size_t>(buf_length);
      }
    }
    if (buf_length) {
      size_t bufsize = static_cast<size_t>(buf_length) * sizeof(int64_t);
      memcpy(ind64.data() + out_offset, buf.data(), bufsize);
      buf_length = 0;
    }
  }
  ind64.resize(out_length);
  length = out_length;
  type = RowIndexType::ARR64;
  set_min_max(ind64);
}


template <typename T>
void ArrayRowIndexImpl::set_min_max(const dt::array<T>& arr) {
  const T* data = arr.data();
  if (length == 0) {
    min = max = 0;
  } else if (is_sorted || length == 1) {
    constexpr int64_t NA = static_cast<int64_t>(GETNA<T>());
    min = static_cast<int64_t>(data[0]);
    max = static_cast<int64_t>(data[length - 1]);
    if (min == NA || max == NA) {
      if (min == NA && max == NA) min = max = 0;
      else if (min == NA) {
        size_t j = 1;
        while (j < length && ISNA<T>(data[j])) ++j;
        min = static_cast<int64_t>(data[j]);
      } else {
        size_t j = length - 2;
        while (j < length && ISNA<T>(data[j])) --j;
        max = static_cast<int64_t>(data[j]);
      }
    }
    if (min > max) std::swap(min, max);
  } else {
    T tmin = std::numeric_limits<T>::max();
    T tmax = -std::numeric_limits<T>::max();
    #pragma omp parallel for schedule(static) \
        reduction(min:tmin) reduction(max:tmax)
    for (size_t j = 0; j < length; ++j) {
      T t = data[j];
      if (ISNA<T>(t)) continue;
      if (t < tmin) tmin = t;
      if (t > tmax) tmax = t;
    }
    min = tmin;
    max = tmax;
    if (min == std::numeric_limits<T>::max() &&
        max == -std::numeric_limits<T>::max()) min = max = 0;
  }
  xassert(min >= 0 && max >= min);
}


void ArrayRowIndexImpl::init_from_boolean_column(BoolColumn* col) {
  const int8_t* data = col->elements_r();
  length = static_cast<size_t>(col->sum());  // total # of 1s in the column

  if (length == 0) {
    // no need to do anything: the data arrays already have 0 length
    type = RowIndexType::ARR32;
    return;
  }
  if (length <= INT32_MAX && col->nrows <= INT32_MAX) {
    type = RowIndexType::ARR32;
    ind32.resize(length);
    size_t k = 0;
    col->rowindex().strided_loop(0, static_cast<int64_t>(col->nrows), 1,
      [&](int64_t i) {
        if (data[i] == 1)
          ind32[k++] = static_cast<int32_t>(i);
      });
    is_sorted = true;
    set_min_max(ind32);
  } else {
    type = RowIndexType::ARR64;
    ind64.resize(length);
    size_t k = 0;
    col->rowindex().strided_loop(0, static_cast<int64_t>(col->nrows), 1,
      [&](int64_t i) {
        if (data[i] == 1)
          ind64[k++] = i;
      });
    is_sorted = true;
    set_min_max(ind64);
  }
}


void ArrayRowIndexImpl::init_from_integer_column(Column* col) {
  if (col->countna()) {
    throw ValueError() << "RowIndex source column contains NA values.";
  }
  min = col->min_int64();
  max = col->max_int64();
  if (min < 0) {
    throw ValueError() << "Row indices in integer column cannot be negative";
  }
  Column* col2 = col->shallowcopy();
  col2->reify();  // noop if col has no rowindex

  length = col->nrows;
  Column* col3 = nullptr;
  if (length <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::ARR32;
    ind32.resize(length);
    // Column cast either converts the data, or memcpy-es it. The `col3`s data
    // will be written into `xbuf`, which is just a view onto `ind32`. Also,
    // since `xbuf` is ExternalMemBuf, its memory won't be reclaimed when
    // the column is destructed.
    MemoryRange xbuf = MemoryRange::external(ind32.data(), length * sizeof(int32_t));
    xassert(xbuf.is_writable());
    col3 = col2->cast(SType::INT32, std::move(xbuf));
  } else {
    type = RowIndexType::ARR64;
    ind64.resize(length);
    MemoryRange xbuf = MemoryRange::external(ind64.data(), length * sizeof(int64_t));
    xassert(xbuf.is_writable());
    col3 = col2->cast(SType::INT64, std::move(xbuf));
  }

  delete col2;
  delete col3;
}


RowIndexImpl* ArrayRowIndexImpl::uplift_from(RowIndexImpl* rii) {
  RowIndexType uptype = rii->type;
  size_t zlen = static_cast<size_t>(length);
  if (uptype == RowIndexType::SLICE) {
    int64_t start = slice_rowindex_get_start(rii);
    int64_t step  = slice_rowindex_get_step(rii);
    arr64_t rowsres(zlen);
    if (type == RowIndexType::ARR32) {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + static_cast<int64_t>(ind32[i]) * step;
      }
    } else {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + ind64[i] * step;
      }
    }
    bool res_sorted = is_sorted && step >= 0;
    auto res = new ArrayRowIndexImpl(std::move(rowsres), res_sorted);
    res->compactify();
    return res;
  }
  if (uptype == RowIndexType::ARR32 && type == RowIndexType::ARR32) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr32_t rowsres(zlen);
    int32_t* rows_ab = arii->ind32.data();
    int32_t* rows_bc = ind32.data();
    for (size_t i = 0; i < zlen; ++i) {
      rowsres[i] = rows_ab[rows_bc[i]];
    }
    bool res_sorted = is_sorted && arii->is_sorted;
    return new ArrayRowIndexImpl(std::move(rowsres), res_sorted);
  }
  if (uptype == RowIndexType::ARR32 || uptype == RowIndexType::ARR64) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr64_t rowsres(zlen);
    if (uptype == RowIndexType::ARR32 && type == RowIndexType::ARR64) {
      int32_t* rows_ab = arii->ind32.data();
      int64_t* rows_bc = ind64.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR32) {
      int64_t* rows_ab = arii->ind64.data();
      int32_t* rows_bc = ind32.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::ARR64 && type == RowIndexType::ARR64) {
      int64_t* rows_ab = arii->ind64.data();
      int64_t* rows_bc = ind64.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    bool res_sorted = is_sorted && arii->is_sorted;
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
  if (max > INT32_MAX || length > INT32_MAX) return;
  xassert(length == ind64.size());

  ind32.resize(length);
  for (size_t i = 0; i < length; ++i) {
    ind32[i] = static_cast<int32_t>(ind64[i]);
  }
  ind64.resize(0);
  type = RowIndexType::ARR32;
}


template <typename TI, typename TO>
RowIndexImpl* ArrayRowIndexImpl::inverse_impl(
    const dt::array<TI>& inputs, size_t nrows) const
{
  size_t newsize = static_cast<size_t>(nrows - length);
  size_t inpsize = inputs.size();
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
      return inverse_impl<int32_t, int32_t>(ind32, nrows);
    else
      return inverse_impl<int32_t, int64_t>(ind32, nrows);
  } else {
    if (nrows <= INT32_MAX)
      return inverse_impl<int64_t, int32_t>(ind64, nrows);
    else
      return inverse_impl<int64_t, int64_t>(ind64, nrows);
  }
}


void ArrayRowIndexImpl::shrink(size_t n) {
  xassert(n < length);
  length = n;
  if (type == RowIndexType::ARR32) {
    ind32.resize(n);
    set_min_max(ind32);
  } else {
    ind64.resize(n);
    set_min_max(ind64);
  }
}

RowIndexImpl* ArrayRowIndexImpl::shrunk(size_t n) {
  xassert(n < length);
  if (type == RowIndexType::ARR32) {
    arr32_t new_ind32(n);
    memcpy(new_ind32.data(), ind32.data(), n * sizeof(int32_t));
    return new ArrayRowIndexImpl(std::move(new_ind32), is_sorted);
  } else {
    arr64_t new_ind64(n);
    memcpy(new_ind64.data(), ind64.data(), n * sizeof(int64_t));
    return new ArrayRowIndexImpl(std::move(new_ind64), is_sorted);
  }
}


int64_t ArrayRowIndexImpl::nth(int64_t i) const {
  size_t zi = static_cast<size_t>(i);
  return (type == RowIndexType::ARR32)? ind32[zi] : ind64[zi];
}



size_t ArrayRowIndexImpl::memory_footprint() const {
  return sizeof(*this) + ind32.size() * 4 + ind64.size() * 8;
}


template <typename T>
static void verify_integrity_helper(
    const dt::array<T>& ind, size_t len, int64_t min, int64_t max, bool sorted)
{
  if (ind.size() != len) {
    throw AssertionError() << "length of data array (" << ind.size()
        << ") does not match the length of the rowindex (" << len << ")";
  }
  T tmin = std::numeric_limits<T>::max();
  T tmax = 0;
  bool check_sorted = sorted;
  for (size_t i = 0; i < len; ++i) {
    T x = ind[i];
    if (ISNA<T>(x)) continue;
    if (x < 0) {
      throw AssertionError()
          << "Element " << i << " in the ArrayRowIndex is negative: " << x;
    }
    if (x < tmin) tmin = x;
    if (x > tmax) tmax = x;
    if (check_sorted && i > 0 && x < ind[i-1]) check_sorted = false;
  }
  if (!len) tmin = 0;
  if (tmin != min || tmax != max) {
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
    verify_integrity_helper<int32_t>(ind32, length, min, max, is_sorted);
  } else if (type == RowIndexType::ARR64) {
    verify_integrity_helper<int64_t>(ind64, length, min, max, is_sorted);
  } else {
    throw AssertionError() << "Invalid type = " << static_cast<int>(type)
        << " in ArrayRowIndex";
  }
  if (ind32 && ind64) {
    throw AssertionError()
        << "ind32 and ind64 are both non-empty in an ArrayRowIndex";
  }
}
