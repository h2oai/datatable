//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <algorithm>           // std::swap, std::move
#include "rowindex.h"
#include "rowindex_impl.h"
#include "utils/assert.h"      // xassert
#include "utils/exceptions.h"  // ValueError, RuntimeError




/**
 * Helper function to verify that
 *     0 <= start, count, start + (count-1)*step <= max
 * (note that `max` is inclusive).
 */
bool check_slice_triple(int64_t start, int64_t count, int64_t step, int64_t max)
{
  // Note: computing `start + step*(count - 1)` may potentially overflow, we
  // must therefore use a safer version.
  return (start >= 0) &&
         (count >= 0) &&
         (step  != 0) &&
         (count <= 1 || step >= -start/(count - 1)) &&
         (count <= 1 || step <= (max - start)/(count - 1));
}

static void _check_triple(int64_t start, int64_t count, int64_t step)
{
  if (!check_slice_triple(start, count, step, INT64_MAX)) {
    throw ValueError() << "Invalid RowIndex slice [" << start << "/"
                       << count << "/" << step << "]";
  }
}



//------------------------------------------------------------------------------
// SliceRowIndexImpl implementation
//------------------------------------------------------------------------------

SliceRowIndexImpl::SliceRowIndexImpl(int64_t i0, int64_t n, int64_t di) {
  _check_triple(i0, n, di);
  type = RowIndexType::SLICE;
  start = i0;
  length = static_cast<size_t>(n);
  step = di;
  if (length == 0) {
    min = max = 0;
  } else {
    min = start;
    max = start + step * (n - 1);
    if (step < 0) std::swap(min, max);
  }
}





int64_t SliceRowIndexImpl::nth(int64_t i) const {
  return start + i * step;
}



RowIndexImpl* SliceRowIndexImpl::uplift_from(RowIndexImpl* rii) {
  RowIndexType uptype = rii->type;
  int64_t ilen = static_cast<int64_t>(length);

  // Product of 2 slices is again a slice.
  if (uptype == RowIndexType::SLICE) {
    SliceRowIndexImpl* uprii = static_cast<SliceRowIndexImpl*>(rii);
    int64_t start_new = uprii->start + uprii->step * start;
    int64_t step_new = uprii->step * step;
    return new SliceRowIndexImpl(start_new, ilen, step_new);
  }

  // Special case: if `step` is 0, then A just contains the same row
  // repeated `length` times, and hence can be created as a slice even
  // if `rii` is an ArrayRowIndex.
  if (step == 0) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    int64_t start_new =
      (uptype == RowIndexType::ARR32)? static_cast<int64_t>(arii->indices32()[start]) :
      (uptype == RowIndexType::ARR64)? arii->indices64()[start] : -1;
    return new SliceRowIndexImpl(start_new, ilen, 0);
  }

  // if C->B is ARR32, then all row indices in C are int32, and thus any
  // valid slice over B will also be ARR32 (except possibly a slice with
  // step = 0 and n > INT32_MAX, which case we handled above).
  if (uptype == RowIndexType::ARR32) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr32_t res(length);
    const int32_t* srcrows = arii->indices32();
    int64_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
  }

  if (uptype == RowIndexType::ARR64) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr64_t res(length);
    const int64_t* srcrows = arii->indices64();
    int64_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
  }

  throw RuntimeError() << "Unknown RowIndexType " << static_cast<int>(uptype);
}



RowIndexImpl* SliceRowIndexImpl::inverse(size_t nrows) const {
  xassert(nrows >= length);
  int64_t newcount = static_cast<int64_t>(nrows - length);
  int64_t tstart = start;
  int64_t tcount = static_cast<int64_t>(length);
  int64_t tstep  = step;
  int64_t irows  = static_cast<int64_t>(nrows);
  if (tstep < 0) {
    tstart += (tcount - 1) * tstep;
    tstep = -tstep;
  }
  if (tstep == 0) {
    tstep = 1;
    tcount = 1;
  }
  if (tstep == 1) {
    if (tstart == 0) {
      return new SliceRowIndexImpl(tcount, newcount, 1);
    }
    if (tstart + tcount == irows) {
      return new SliceRowIndexImpl(0, newcount, 1);
    }
    arr64_t starts(2);
    arr64_t counts(2);
    arr64_t steps(2);
    starts[0] = 0;
    counts[0] = tstart;
    steps[0] = 1;
    starts[1] = tstart + tcount;
    counts[1] = irows - (tstart + tcount);
    steps[1] = 1;
    return new ArrayRowIndexImpl(starts, counts, steps);
  }
  size_t znewcount = static_cast<size_t>(newcount);
  size_t j = 0;
  if (irows <= INT32_MAX) {
    arr32_t indices(znewcount);
    int32_t nrows32 = static_cast<int32_t>(irows);
    int32_t tstep32 = static_cast<int32_t>(tstep);
    int32_t next_row_to_skip = static_cast<int32_t>(tstart);
    int32_t tend = static_cast<int32_t>(tstart + tcount * tstep);
    for (int32_t i = 0; i < nrows32; ++i) {
      if (i == next_row_to_skip) {
        next_row_to_skip += tstep32;
        if (next_row_to_skip == tend) next_row_to_skip = nrows32;
        continue;
      } else {
        indices[j++] = i;
      }
    }
    xassert(j == znewcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  } else {
    arr64_t indices(znewcount);
    int64_t next_row_to_skip = tstart;
    int64_t tend = tstart + tcount * tstep;
    for (int64_t i = 0; i < irows; ++i) {
      if (i == next_row_to_skip) {
        next_row_to_skip += tstep;
        if (next_row_to_skip == tend) next_row_to_skip = irows;
        continue;
      } else {
        indices[j++] = i;
      }
    }
    xassert(j == znewcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  }
}


void SliceRowIndexImpl::shrink(size_t n) {
  length = n;
  if (step > 0) max = start + step * static_cast<int64_t>(n - 1);
  if (step < 0) min = start + step * static_cast<int64_t>(n - 1);
}

RowIndexImpl* SliceRowIndexImpl::shrunk(size_t n) {
  return new SliceRowIndexImpl(start, static_cast<int64_t>(n), step);
}



size_t SliceRowIndexImpl::memory_footprint() const {
  return sizeof(*this);
}



void SliceRowIndexImpl::verify_integrity() const {
  RowIndexImpl::verify_integrity();

  if (type != RowIndexType::SLICE) {
    throw AssertionError() << "Invalid type = " << static_cast<int>(type)
        << " in a SliceRowIndex";
  }

  try {
    _check_triple(start, static_cast<int64_t>(length), step);
  } catch (const Error&) {
    throw AssertionError()
        << "Invalid slice rowindex: " << start << "/" << length << "/" << step;
  }

  if (length > 0) {
    int64_t minrow = start;
    int64_t maxrow = start + step * static_cast<int64_t>(length - 1);
    if (step < 0) std::swap(minrow, maxrow);
    if (min != minrow || max != maxrow) {
      throw AssertionError()
          << "Invalid min/max values in a Slice RowIndex " << start << "/"
          << length << "/" << step << ": min = " << min << ", max = " << max;
    }
  }
}


int64_t slice_rowindex_get_start(const RowIndexImpl* impl) {
  auto simpl = dynamic_cast<const SliceRowIndexImpl*>(impl);
  return simpl->start;
}
int64_t slice_rowindex_get_step(const RowIndexImpl* impl) {
  auto simpl = dynamic_cast<const SliceRowIndexImpl*>(impl);
  return simpl->step;
}
