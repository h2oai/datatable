//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <algorithm>           // std::swap, std::move
#include "column/range.h"
#include "utils/assert.h"      // xassert
#include "utils/exceptions.h"  // ValueError, RuntimeError
#include "utils/macros.h"
#include "rowindex.h"
#include "rowindex_impl.h"




/**
 * Helper function to verify that
 *     0 <= start, count, start + (count-1)*step <= max
 * (note that `max` is inclusive).
 */
bool check_slice_triple(size_t start, size_t count, size_t step, size_t max)
{
  // Note: computing `start + step*(count - 1)` may potentially overflow, we
  // must therefore use a safer version.

  return (start <= max) &&
         (count <= RowIndex::MAX) &&
         (count <= 1 || (step == 0) ||
         (step <= RowIndex::MAX? step <= (max - start)/(count - 1)
                               : 0 - step <= start/(count - 1)));
}



//------------------------------------------------------------------------------
// SliceRowIndexImpl implementation
//------------------------------------------------------------------------------

SliceRowIndexImpl::SliceRowIndexImpl(size_t i0, size_t n, size_t di) {
  xassert(check_slice_triple(i0, n, di, RowIndex::MAX));
  type   = RowIndexType::SLICE;
  ascending = (di <= RowIndex::MAX);
  start  = i0;
  length = n;
  step   = di;
  if (length == 0) {
    max_valid = false;
  } else {
    max = ascending? start + step * (n - 1) : start;
    max_valid = true;
  }
}


bool SliceRowIndexImpl::get_element(size_t i, size_t* out) const {
  *out = start + i * step;
  return true;
}


Column SliceRowIndexImpl::as_column() const {
  return Column(new dt::Range_ColumnImpl(static_cast<int64_t>(start),
                                         static_cast<int64_t>(length),
                                         static_cast<int64_t>(step),
                                         dt::Type()));
}


RowIndexImpl* SliceRowIndexImpl::uplift_from(const RowIndexImpl* rii) const {
  RowIndexType uptype = rii->type;

  // Product of 2 slices is again a slice.
  if (uptype == RowIndexType::SLICE) {
    auto uprii = static_cast<const SliceRowIndexImpl*>(rii);
    size_t start_new = uprii->start + uprii->step * start;
    size_t step_new = uprii->step * step;
    return new SliceRowIndexImpl(start_new, length, step_new);
  }

  // Special case: if `step` is 0, then A just contains the same row
  // repeated `length` times, and hence can be created as a slice even
  // if `rii` is an ArrayRowIndex.
  if (step == 0) {
    size_t start_new;
    bool start_valid = rii->get_element(start, &start_new);
    if (start_valid) {
      return new SliceRowIndexImpl(start_new, length, 0);
    }
  }

  // if C->B is ARR32, then all row indices in C are int32, and thus any
  // valid slice over B will also be ARR32 (except possibly a slice with
  // step = 0 and n > INT32_MAX, which case we handled above).
  if (uptype == RowIndexType::ARR32) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    Buffer outbuf = Buffer::mem(length * sizeof(int32_t));
    auto res = static_cast<int32_t*>(outbuf.xptr());
    const int32_t* srcrows = arii->indices32();
    size_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(outbuf), RowIndex::ARR32);
  }

  if (uptype == RowIndexType::ARR64) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    Buffer outbuf = Buffer::mem(length * sizeof(int64_t));
    auto res = static_cast<int64_t*>(outbuf.xptr());
    const int64_t* srcrows = arii->indices64();
    size_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(outbuf), RowIndex::ARR64);
  }

  throw RuntimeError() << "Unknown RowIndexType " << static_cast<int>(uptype);
}



RowIndexImpl* SliceRowIndexImpl::negate(size_t nrows) const {
  xassert(nrows >= length);
  size_t newcount = nrows - length;
  size_t tstart = start;
  size_t tcount = length;
  size_t tstep  = step;
  if (!ascending) {  // negative step
    tstart += (tcount - 1) * tstep;
    tstep = 0 - tstep;
  }
  if (tstep == 1) {
    if (tstart == 0) {
      return new SliceRowIndexImpl(tcount, newcount, 1);
    }
    if (tstart + tcount == nrows) {
      return new SliceRowIndexImpl(0, newcount, 1);
    }
  }
  if (nrows <= INT32_MAX) {
    size_t j = 0;
    Buffer outbuf = Buffer::mem(newcount * sizeof(int32_t));
    auto indices = static_cast<int32_t*>(outbuf.xptr());
    if (tstep == 1) {
      for (size_t i = 0; i < tstart; ++i) {
        indices[j++] = static_cast<int32_t>(i);
      }
      for (size_t i = tstart + tcount; i < nrows; ++i) {
        indices[j++] = static_cast<int32_t>(i);
      }
    } else {
      size_t next_row_to_skip = tstart;
      size_t tend = tstart + tcount * tstep;
      for (size_t i = 0; i < nrows; ++i) {
        if (i == next_row_to_skip) {
          next_row_to_skip += tstep;
          if (next_row_to_skip == tend) next_row_to_skip = nrows;
          continue;
        } else {
          indices[j++] = static_cast<int32_t>(i);
        }
      }
    }
    xassert(j == newcount);
    return new ArrayRowIndexImpl(std::move(outbuf),
                                 RowIndex::ARR32|RowIndex::SORTED);
  } else {
    size_t j = 0;
    Buffer outbuf = Buffer::mem(newcount * sizeof(int64_t));
    auto indices = static_cast<int64_t*>(outbuf.xptr());
    if (tstep == 1) {
      for (size_t i = 0; i < tstart; ++i) {
        indices[j++] = static_cast<int64_t>(i);
      }
      for (size_t i = tstart + tcount; i < nrows; ++i) {
        indices[j++] = static_cast<int64_t>(i);
      }
    } else {
      size_t next_row_to_skip = tstart;
      size_t tend = tstart + tcount * tstep;
      for (size_t i = 0; i < nrows; ++i) {
        if (i == next_row_to_skip) {
          next_row_to_skip += tstep;
          if (next_row_to_skip == tend) next_row_to_skip = nrows;
          continue;
        } else {
          indices[j++] = static_cast<int64_t>(i);
        }
      }
    }
    xassert(j == newcount);
    return new ArrayRowIndexImpl(std::move(outbuf),
                                 RowIndex::ARR64|RowIndex::SORTED);
  }
}




size_t SliceRowIndexImpl::memory_footprint() const noexcept {
  return sizeof(*this);
}



void SliceRowIndexImpl::verify_integrity() const {
  RowIndexImpl::verify_integrity();

  XAssert(type == RowIndexType::SLICE);
  XAssert(check_slice_triple(start, length, step, RowIndex::MAX));

  if (length > 0) {
    size_t minrow = start;
    size_t maxrow = start + step * (length - 1);
    if (!ascending) std::swap(minrow, maxrow);
    XAssert(max == maxrow);
    XAssert(ascending == (step <= RowIndex::MAX));
  }
}


size_t slice_rowindex_get_start(const RowIndexImpl* impl) noexcept {
  auto simpl = dynamic_cast<const SliceRowIndexImpl*>(impl);
  return simpl? simpl->start : 0;
}
size_t slice_rowindex_get_step(const RowIndexImpl* impl) noexcept {
  auto simpl = dynamic_cast<const SliceRowIndexImpl*>(impl);
  return simpl? simpl->step : 0;
}

bool slice_rowindex_increasing(const RowIndexImpl* impl) noexcept {
  auto simpl = dynamic_cast<const SliceRowIndexImpl*>(impl);
  return simpl? simpl->ascending : false;
}
