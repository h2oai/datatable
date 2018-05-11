//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "rowindex.h"
#include <algorithm>           // std::min, std::swap, std::move
#include <cstdlib>             // std::memcpy
#include <limits>              // std::numeric_limits
#include "column.h"            // Column, BoolColumn
#include "datatable_check.h"   // IntegrityCheckContext
#include "memorybuf.h"         // MemoryBuffer, ExternalMemBuf
#include "utils/exceptions.h"  // ValueError, RuntimeError
#include "utils/assert.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// ArrayRowIndexImpl implementation
//------------------------------------------------------------------------------

ArrayRowIndexImpl::ArrayRowIndexImpl(arr32_t&& array, bool sorted)
    : ind32(std::move(array)), ind64()
{
  type = RowIndexType::RI_ARR32;
  length = static_cast<int64_t>(ind32.size());
  xassert(length <= std::numeric_limits<int32_t>::max());
  set_min_max<int32_t>(ind32, sorted);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(arr64_t&& array, bool sorted)
    : ind32(), ind64(std::move(array))
{
  type = RowIndexType::RI_ARR64;
  length = static_cast<int64_t>(ind64.size());
  set_min_max<int64_t>(ind64, sorted);
}


// Construct from a list of slices
ArrayRowIndexImpl::ArrayRowIndexImpl(
    const arr64_t& starts, const arr64_t& counts, const arr64_t& steps)
{
  size_t n = starts.size();
  xassert(n == counts.size() && n == steps.size());

  // Compute the total number of elements, and the largest index that needs
  // to be stored. Also check for potential overflows / invalid values.
  length = 0;
  min = INT64_MAX;
  max = 0;
  for (size_t i = 0; i < n; ++i) {
    int64_t start = starts[i];
    int64_t step = steps[i];
    int64_t len = counts[i];
    SliceRowIndexImpl::check_triple(start, len, step);
    if (len == 0) continue;
    int64_t end = start + step * (len - 1);
    if (start < min) min = start;
    if (start > max) max = start;
    if (end < min) min = end;
    if (end > max) max = end;
    length += len;
  }
  if (max == 0) min = 0;
  xassert(min >= 0 && min <= max);
  size_t zlen = static_cast<size_t>(length);

  if (zlen <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::RI_ARR32;
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
    type = RowIndexType::RI_ARR64;
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
  switch (col->stype()) {
    case ST_BOOLEAN_I1:
      init_from_boolean_column(static_cast<BoolColumn*>(col));
      break;
    case ST_INTEGER_I1:
    case ST_INTEGER_I2:
    case ST_INTEGER_I4:
    case ST_INTEGER_I8:
      init_from_integer_column(col);
      break;
    default:
      throw ValueError() << "Column is not of boolean or integer type";
  }
}


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn32* ff, int64_t n, bool sorted) {
  xassert(n <= std::numeric_limits<int32_t>::max());

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
        out_length += (size_t) buf_length;
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
  length = static_cast<int64_t>(out_length);
  type = RowIndexType::RI_ARR32;
  set_min_max(ind32, sorted);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(filterfn64* ff, int64_t n, bool sorted) {
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
  length = static_cast<int64_t>(out_length);
  type = RowIndexType::RI_ARR64;
  set_min_max(ind64, sorted);
}


template <typename T>
void ArrayRowIndexImpl::set_min_max(const dt::array<T>& arr, bool sorted) {
  const T* data = arr.data();
  if (length <= 1) {
    min = max = (length == 0) ? 0 : static_cast<T>(data[0]);
  } else if (sorted) {
    min = static_cast<T>(data[0]);
    max = static_cast<T>(data[length - 1]);
    if (min > max) std::swap(min, max);
  } else {
    T tmin = std::numeric_limits<T>::max();
    T tmax = -std::numeric_limits<T>::max();
    #pragma omp parallel for schedule(static) \
        reduction(min:tmin) reduction(max:tmax)
    for (int64_t j = 0; j < length; ++j) {
      T t = data[j];
      if (t < tmin) tmin = t;
      if (t > tmax) tmax = t;
    }
    min = tmin;
    max = tmax;
  }
}


void ArrayRowIndexImpl::init_from_boolean_column(BoolColumn* col) {
  int8_t* data = col->elements();
  length = col->sum();  // total # of 1s in the column
  size_t zlen = static_cast<size_t>(length);

  if (length == 0) {
    // no need to do anything: the data arrays already have 0 length
    type = RowIndexType::RI_ARR32;
    return;
  }
  if (length <= INT32_MAX && col->nrows <= INT32_MAX) {
    type = RowIndexType::RI_ARR32;
    ind32.resize(zlen);
    size_t k = 0;
    col->rowindex().strided_loop(0, col->nrows, 1,
      [&](int64_t i) {
        if (data[i] == 1)
          ind32[k++] = static_cast<int32_t>(i);
      });
    set_min_max(ind32, true);
  } else {
    type = RowIndexType::RI_ARR64;
    ind64.resize(zlen);
    size_t k = 0;
    col->rowindex().strided_loop(0, col->nrows, 1,
      [&](int64_t i) {
        if (data[i] == 1)
          ind64[k++] = i;
      });
    set_min_max(ind64, true);
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
  size_t zn = static_cast<size_t>(length);
  Column* col3 = nullptr;
  if (length <= INT32_MAX && max <= INT32_MAX) {
    type = RowIndexType::RI_ARR32;
    ind32.resize(zn);
    MemoryBuffer* xbuf = new ExternalMemBuf(ind32.data(), zn * 4);
    // Column cast either converts the data, or memcpy-es it. The `col3`s data
    // will be written into `xbuf`, which is just a view onto `ind32`. Also,
    // since `xbuf` is ExternalMemBuf, its memory won't be reclaimed when
    // the column is destructed.
    col3 = col2->cast(ST_INTEGER_I4, xbuf);
  } else {
    type = RowIndexType::RI_ARR64;
    ind64.resize(zn);
    MemoryBuffer* xbuf = new ExternalMemBuf(ind64.data(), zn * 8);
    col3 = col2->cast(ST_INTEGER_I8, xbuf);
  }

  delete col2;
  delete col3;
}


RowIndexImpl* ArrayRowIndexImpl::uplift_from(RowIndexImpl* rii) {
  RowIndexType uptype = rii->type;
  size_t zlen = static_cast<size_t>(length);
  if (uptype == RowIndexType::RI_SLICE) {
    SliceRowIndexImpl* srii = static_cast<SliceRowIndexImpl*>(rii);
    int64_t start = srii->start;
    int64_t step  = srii->step;
    arr64_t rowsres(zlen);
    if (type == RowIndexType::RI_ARR32) {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + static_cast<int64_t>(ind32[i]) * step;
      }
    } else {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + ind64[i] * step;
      }
    }
    auto res = new ArrayRowIndexImpl(std::move(rowsres), false);
    res->compactify();
    return res;
  }
  if (uptype == RowIndexType::RI_ARR32 && type == RowIndexType::RI_ARR32) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr32_t rowsres(zlen);
    int32_t* rows_ab = arii->ind32.data();
    int32_t* rows_bc = ind32.data();
    for (size_t i = 0; i < zlen; ++i) {
      rowsres[i] = rows_ab[rows_bc[i]];
    }
    return new ArrayRowIndexImpl(std::move(rowsres), false);
  }
  if (uptype == RowIndexType::RI_ARR32 || uptype == RowIndexType::RI_ARR64) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr64_t rowsres(zlen);
    if (uptype == RowIndexType::RI_ARR32 && type == RowIndexType::RI_ARR64) {
      int32_t* rows_ab = arii->ind32.data();
      int64_t* rows_bc = ind64.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::RI_ARR64 && type == RowIndexType::RI_ARR32) {
      int64_t* rows_ab = arii->ind64.data();
      int32_t* rows_bc = ind32.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    if (uptype == RowIndexType::RI_ARR64 && type == RowIndexType::RI_ARR64) {
      int64_t* rows_ab = arii->ind64.data();
      int64_t* rows_bc = ind64.data();
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = rows_ab[rows_bc[i]];
      }
    }
    auto res = new ArrayRowIndexImpl(std::move(rowsres), false);
    res->compactify();
    return res;
  }
  throw RuntimeError() << "Unknown RowIndexType " << uptype;
}


/**
 * Attempt to convert an ARR64 RowIndex object into the ARR32 format. If such
 * conversion is possible, the object will be modified in-place (regardless of
 * its refcount).
 */
void ArrayRowIndexImpl::compactify()
{
  if (type == RowIndexType::RI_ARR32) return;
  if (max > INT32_MAX || length > INT32_MAX) return;
  size_t zlen = static_cast<size_t>(length);
  xassert(zlen == ind64.size());

  ind32.resize(zlen);
  for (size_t i = 0; i < zlen; ++i) {
    ind32[i] = static_cast<int32_t>(ind64[i]);
  }
  ind64.resize(0);
  type = RowIndexType::RI_ARR32;
}


template <typename TI, typename TO>
RowIndexImpl* ArrayRowIndexImpl::inverse_impl(
    const dt::array<TI>& inputs, int64_t nrows) const
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


RowIndexImpl* ArrayRowIndexImpl::inverse(int64_t nrows) const {
  xassert(nrows >= length);
  if (type == RowIndexType::RI_ARR32) {
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


void ArrayRowIndexImpl::shrink(int64_t n) {
  xassert(n < length);
  if (type == RowIndexType::RI_ARR32) {
    ind32.resize(static_cast<size_t>(n));
  } else {
    ind64.resize(static_cast<size_t>(n));
  }
  length = n;
}

RowIndexImpl* ArrayRowIndexImpl::shrunk(int64_t n) {
  xassert(n < length);
  size_t zn = static_cast<size_t>(n);
  if (type == RowIndexType::RI_ARR32) {
    arr32_t new_ind32(zn);
    memcpy(new_ind32.data(), ind32.data(), zn * sizeof(int32_t));
    return new ArrayRowIndexImpl(std::move(new_ind32), false);
  } else {
    arr64_t new_ind64(zn);
    memcpy(new_ind64.data(), ind64.data(), zn * sizeof(int64_t));
    return new ArrayRowIndexImpl(std::move(new_ind64), false);
  }
}


int64_t ArrayRowIndexImpl::nth(int64_t i) const {
  size_t zi = static_cast<size_t>(i);
  return (type == RowIndexType::RI_ARR32)? ind32[zi] : ind64[zi];
}



size_t ArrayRowIndexImpl::memory_footprint() const {
  return sizeof(*this) + ind32.size() * 4 + ind64.size() * 8;
}



bool ArrayRowIndexImpl::verify_integrity(IntegrityCheckContext& icc) const {
  if (!RowIndexImpl::verify_integrity(icc)) return false;
  auto end = icc.end();
  size_t zlen = static_cast<size_t>(length);

  if (type != RowIndexType::RI_ARR32 && type != RowIndexType::RI_ARR64) {
    icc << "Invalid type = " << type << " in ArrayRowIndex" << end;
    return false;
  }

  if (type == RowIndexType::RI_ARR32) {
    if (ind64) {
      icc << "ind64 array has size " << ind64.size() << " in Array32 RowIndex"
          << end;
      return false;
    }
    if (ind32.size() != zlen) {
      icc << "length of ind32 array (" << ind32.size() << ") does not match "
          << "the length of the rowindex (" << zlen << ")" << end;
      return false;
    }
    int32_t tmin = std::numeric_limits<int32_t>::max();
    int32_t tmax = 0;
    for (size_t i = 0; i < zlen; ++i) {
      int32_t x = ind32[i];
      if (ISNA<int32_t>(x)) {
        icc << "Element " << i << " in the ArrayRowIndex is NA" << end;
      }
      if (x < 0) {
        icc << "Element " << i << " in the ArrayRowIndex is negative: "
            << x << end;
      }
      if (x < tmin) tmin = x;
      if (x > tmax) tmax = x;
    }
    if (tmin != min || tmax != max) {
      icc << "Mismatching min/max values in the ArrayRowIndex (" << min
          << "/" << max << ") compared to the computed ones (" << tmin
          << "/" << tmax << ")" << end;
    }
  }

  if (type == RowIndexType::RI_ARR64) {
    if (ind32) {
      icc << "ind32 array has size " << ind32.size() << " in Array64 RowIndex"
          << end;
      return false;
    }
    if (ind64.size() != zlen) {
      icc << "length of ind64 array (" << ind64.size() << ") does not match "
          << "the length of the rowindex (" << zlen << ")" << end;
      return false;
    }
    int64_t tmin = std::numeric_limits<int64_t>::max();
    int64_t tmax = 0;
    if (zlen == 0) tmin = tmax = 0;
    for (size_t i = 0; i < zlen; ++i) {
      int64_t x = ind64[i];
      if (ISNA<int64_t>(x)) {
        icc << "Element " << i << " in the ArrayRowIndex is NA" << end;
      }
      if (x < 0) {
        icc << "Element " << i << " in the ArrayRowIndex is negative: "
            << x << end;
      }
      if (x < tmin) tmin = x;
      if (x > tmax) tmax = x;
    }
    if (tmin != min || tmax != max) {
      icc << "Mismatching min/max values in the ArrayRowIndex (" << min
          << "/" << max << ") compared to the computed ones (" << tmin
          << "/" << tmax << ")" << end;
    }
  }

  return true;
}
