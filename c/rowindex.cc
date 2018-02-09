//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "rowindex.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>   // std::min, std::swap
#include <cstring>     // std::memcpy
#include <limits>      // std::numeric_limits
#include "column.h"
#include "datatable.h"
#include "datatable_check.h"
#include "types.h"
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


/**
 * Construct a RowIndex object from triple `(start, count, step)`. The new
 * object will have type `RI_SLICE`.
 *
 * Note that we depart from Python's standard of using `(start, end, step)` to
 * denote a slice -- having a `count` gives several advantages:
 *   - computing the "end" is easy and unambiguous: `start + count * step`;
 *     whereas computing "count" from `end` is harder: `(end - start) / step`.
 *   - with explicit `count` the `step` may safely be 0.
 *   - there is no difference in handling positive/negative steps.
 */
RowIndex RowIndex::from_slice(int64_t start, int64_t count, int64_t step) {
  return RowIndex(new SliceRowIndexImpl(start, count, step));
}


/**
 * Construct an "array" `RowIndex` object from a series of triples
 * `(start, count, step)`. The triples are given as 3 separate arrays of starts,
 * of counts and of steps.
 *
 * This will create either an RI_ARR32 or RI_ARR64 object, depending on which
 * one is sufficient to hold all the indices.
 */
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



//==============================================================================
// SliceRowIndexImpl
//==============================================================================

SliceRowIndexImpl::SliceRowIndexImpl(int64_t i0, int64_t n, int64_t di) {
  SliceRowIndexImpl::check_triple(i0, n, di);
  type = RowIndexType::RI_SLICE;
  start = i0;
  length = n;
  step = di;
  if (length == 0) {
    min = max = 0;
  } else {
    min = start;
    max = start + step * (length - 1);
    if (step < 0) std::swap(min, max);
  }
}


/**
 * Helper function to verify that
 *     0 <= start, count, start + (count-1)*step <= INT64_MAX
 *
 * If one of these conditions fails, throws an exception, otherwise does
 * nothing.
 */
void SliceRowIndexImpl::check_triple(int64_t start, int64_t count, int64_t step)
{
  if (start < 0 || count < 0 ||
      (count > 1 && step < -(start/(count - 1))) ||
      (count > 1 && step > (INT64_MAX - start)/(count - 1))) {
    throw ValueError() << "Invalid RowIndex slice [" << start << "/"
                       << count << "/" << step << "]";
  }
}


RowIndexImpl* SliceRowIndexImpl::uplift_from(RowIndexImpl* rii) {
  RowIndexType uptype = rii->type;
  size_t zlen = static_cast<size_t>(length);
  if (uptype == RI_SLICE) {
    // Product of 2 slices is again a slice.
    SliceRowIndexImpl* uprii = static_cast<SliceRowIndexImpl*>(rii);
    int64_t start_new = uprii->start + uprii->step * start;
    int64_t step_new = uprii->step * step;
    return new SliceRowIndexImpl(start_new, length, step_new);
  }
  if (step == 0) {
    // Special case: if `step` is 0, then A just contains the same row
    // repeated `length` times, and hence can be created as a slice even
    // if `rii` is an ArrayRowIndex.
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    int64_t start_new =
      (uptype == RI_ARR32)? static_cast<int64_t>(arii->indices32()[start]) :
      (uptype == RI_ARR64)? arii->indices64()[start] : -1;
    return new SliceRowIndexImpl(start_new, length, 0);
  }
  if (uptype == RI_ARR32) {
    // if C->B is ARR32, then all row indices in C are int32, and thus any
    // valid slice over B will also be ARR32 (except possibly a slice with
    // step = 0 and n > INT32_MAX, which case we handled above).
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    dt::array<int32_t> res(zlen);
    const int32_t* srcrows = arii->indices32();
    int64_t j = start;
    for (size_t i = 0; i < zlen; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
  }
  if (uptype == RI_ARR64) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    dt::array<int64_t> res(zlen);
    const int64_t* srcrows = arii->indices64();
    int64_t j = start;
    for (size_t i = 0; i < zlen; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
  }
  throw RuntimeError() << "Unknown RowIndexType " << uptype;
}



//==============================================================================
// ArrayRowIndexImpl
//==============================================================================

ArrayRowIndexImpl::ArrayRowIndexImpl(dt::array<int32_t>&& array, bool sorted)
    : ind32(std::move(array)), ind64()
{
  type = RowIndexType::RI_ARR32;
  length = static_cast<int64_t>(ind32.size());
  assert(length <= std::numeric_limits<int32_t>::max());
  set_min_max<int32_t>(ind32, sorted);
}


ArrayRowIndexImpl::ArrayRowIndexImpl(dt::array<int64_t>&& array, bool sorted)
    : ind32(), ind64(std::move(array))
{
  type = RowIndexType::RI_ARR64;
  length = static_cast<int64_t>(ind64.size());
  set_min_max<int64_t>(ind64, sorted);
}


// Construct from a list of slices
ArrayRowIndexImpl::ArrayRowIndexImpl(
    const dt::array<int64_t>& starts,
    const dt::array<int64_t>& counts,
    const dt::array<int64_t>& steps)
{
  size_t n = starts.size();
  assert(n == counts.size() && n == steps.size());

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
  assert(min >= 0 && min <= max);
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
    assert(rowsptr == &ind32[zlen]);
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
    assert(rowsptr == &ind64[zlen]);
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
  assert(n <= std::numeric_limits<int32_t>::max());

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
    dt::array<int32_t> buf(zrows_per_chunk);

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
      memcpy(ind32.data() + out_offset, buf.data(), bufsize);
      buf_length = 0;
    }
  }

  // In the end we shrink the output buffer to the size corresponding to the
  // actual number of elements written.
  ind32.resize(out_length);
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
    dt::array<int64_t> buf(zrows_per_chunk);
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
  int64_t nouts = col->sum();  // total # of 1s in the column
  size_t zn = static_cast<size_t>(nouts);
  if (nouts == 0) {
    // no need to do anything: the data arrays already have 0 length
    type = RowIndexType::RI_ARR32;
    return;
  }
  if (nouts <= INT32_MAX && col->nrows <= INT32_MAX) {
    type = RowIndexType::RI_ARR32;
    ind32.resize(zn);
    size_t k = 0;
    col->rowindex().strided_loop(0, col->nrows, 1,
      [&](int64_t i) {
        if (data[i] == 1)
          ind32[k++] = static_cast<int32_t>(i);
      });
    set_min_max(ind32, true);
  } else {
    type = RowIndexType::RI_ARR64;
    ind64.resize(zn);
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
    dt::array<int64_t> rowsres(zlen);
    if (type == RowIndexType::RI_ARR32) {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + static_cast<int64_t>(ind32[i]) * step;
      }
    } else {
      for (size_t i = 0; i < zlen; ++i) {
        rowsres[i] = start + ind64[i] * step;
      }
    }
    // res->compactify();
    return new ArrayRowIndexImpl(std::move(rowsres), false);
  }
  if (uptype == RowIndexType::RI_ARR32 && type == RowIndexType::RI_ARR32) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    dt::array<int32_t> rowsres(zlen);
    int32_t* rows_ab = arii->ind32.data();
    int32_t* rows_bc = ind32.data();
    for (size_t i = 0; i < zlen; ++i) {
      rowsres[i] = rows_ab[rows_bc[i]];
    }
    return new ArrayRowIndexImpl(std::move(rowsres), false);
  }
  if (uptype == RowIndexType::RI_ARR32 || uptype == RowIndexType::RI_ARR64) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    dt::array<int64_t> rowsres(zlen);
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
    // res->compactify();
    return new ArrayRowIndexImpl(std::move(rowsres), false);
  }
  throw RuntimeError() << "Unknown RowIndexType " << uptype;
}



















































//============================================================================



/**
 * Attempt to convert an ARR64 RowIndex object into the ARR32 format. If such
 * conversion is possible, the object will be modified in-place (regardless of
 * its refcount).
 */
/*
void RowIndex::compactify()
{
  if (type != RI_ARR64 || _max > INT32_MAX || _length > INT32_MAX) return;

  int64_t* src = ind64;
  int32_t* res = (int32_t*) src;  // Note: res writes on top of src!
  int32_t len = (int32_t) _length;
  for (int32_t i = 0; i < len; i++) {
    res[i] = (int32_t) src[i];
  }
  dtrealloc_g(res, int32_t, len);
  type = RI_ARR32;
  ind32 = res;
  fail: {}
}
*/



/**
 * Convert a slice RowIndex into an RI_ARR32/RI_ARR64.
 */
  /*
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


/*
size_t RowIndex::alloc_size()
{
  size_t sz = sizeof(*this);
  switch (type) {
    case RI_ARR32: sz += (size_t)_length * sizeof(int32_t); break;
    case RI_ARR64: sz += (size_t)_length * sizeof(int64_t); break;
    default: break;
  }
  return sz;
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

  // Check that rowindex length is valid
  if (_length < 0) {
    icc << name << ".length is negative: " << _length << end;
    return false;
  }
  // Check for a positive refcount
  if (refcount <= 0) {
    icc << name << " has a nonpositive refcount: " << refcount << end;
  }

  int64_t maxrow = -INT64_MAX;
  int64_t minrow = INT64_MAX;

  switch (type) {
    case RI_SLICE: {
      int64_t start = slice.start;
      // Check that the starting row is not negative
      if (start < 0) {
        icc << "Starting row of " << name << " is negative: " << slice.start
          << end;
        return false;
      }
      int64_t step = slice.step;
      // Ensure that the last index won't lead to an overflow or a negative value
      if (_length > 1) {
        if (step > (INT64_MAX - start) / (_length - 1)) {
        icc << "Slice in " << name << " leads to integer overflow: start = "
          << start << ", step = " << step << ", length = " << _length << end;
        return false;
        }
        if (step < -start / (_length - 1)) {
        icc << "Slice in " << name << " has negative indices: start = " << start
          << ", step = " << step << ", length = " << _length << end;
        return false;
        }
      }
      int64_t sliceend = start + step * (_length - 1);
      maxrow = step > 0 ? sliceend : start;
      minrow = step > 0 ? start : sliceend;
      break;
    }
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
