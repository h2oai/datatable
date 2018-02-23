//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "rowindex.h"
#include <algorithm>           // std::swap, std::move
#include "datatable_check.h"   // IntegrityCheckContext
#include "utils/exceptions.h"  // ValueError, RuntimeError



//------------------------------------------------------------------------------
// SliceRowIndexImpl implementation
//------------------------------------------------------------------------------

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

  // Product of 2 slices is again a slice.
  if (uptype == RI_SLICE) {
    SliceRowIndexImpl* uprii = static_cast<SliceRowIndexImpl*>(rii);
    int64_t start_new = uprii->start + uprii->step * start;
    int64_t step_new = uprii->step * step;
    return new SliceRowIndexImpl(start_new, length, step_new);
  }

  // Special case: if `step` is 0, then A just contains the same row
  // repeated `length` times, and hence can be created as a slice even
  // if `rii` is an ArrayRowIndex.
  if (step == 0) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    int64_t start_new =
      (uptype == RI_ARR32)? static_cast<int64_t>(arii->indices32()[start]) :
      (uptype == RI_ARR64)? arii->indices64()[start] : -1;
    return new SliceRowIndexImpl(start_new, length, 0);
  }

  // if C->B is ARR32, then all row indices in C are int32, and thus any
  // valid slice over B will also be ARR32 (except possibly a slice with
  // step = 0 and n > INT32_MAX, which case we handled above).
  if (uptype == RI_ARR32) {
    ArrayRowIndexImpl* arii = static_cast<ArrayRowIndexImpl*>(rii);
    arr32_t res(zlen);
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
    arr64_t res(zlen);
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



RowIndexImpl* SliceRowIndexImpl::inverse(int64_t nrows) const {
  assert(nrows >= length);
  int64_t newcount = nrows - length;
  int64_t tstart = start;
  int64_t tcount = length;
  int64_t tstep  = step;
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
    if (tstart + tcount == nrows) {
      return new SliceRowIndexImpl(0, newcount, 1);
    }
    arr64_t starts(2);
    arr64_t counts(2);
    arr64_t steps(2);
    starts[0] = 0;
    counts[0] = tstart;
    steps[0] = 1;
    starts[1] = tstart + tcount;
    counts[1] = nrows - (tstart + tcount);
    steps[1] = 1;
    return new ArrayRowIndexImpl(starts, counts, steps);
  }
  size_t znewcount = static_cast<size_t>(newcount);
  size_t j = 0;
  if (nrows <= INT32_MAX) {
    arr32_t indices(znewcount);
    int32_t nrows32 = static_cast<int32_t>(nrows);
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
    assert(j == znewcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  } else {
    arr64_t indices(znewcount);
    int64_t next_row_to_skip = tstart;
    int64_t tend = tstart + tcount * tstep;
    for (int64_t i = 0; i < nrows; ++i) {
      if (i == next_row_to_skip) {
        next_row_to_skip += tstep;
        if (next_row_to_skip == tend) next_row_to_skip = nrows;
        continue;
      } else {
        indices[j++] = i;
      }
    }
    assert(j == znewcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  }
}



size_t SliceRowIndexImpl::memory_footprint() const {
  return sizeof(*this);
}



bool SliceRowIndexImpl::verify_integrity(IntegrityCheckContext& icc) const {
  if (!RowIndexImpl::verify_integrity(icc)) return false;
  auto end = icc.end();

  if (type != RowIndexType::RI_SLICE) {
    icc << "Invalid type = " << type << " in a SliceRowIndex" << end;
    return false;
  }

  try {
    check_triple(start, length, step);
  } catch (const Error&) {
    icc << "Invalid slice rowindex: " << start << "/" << length << "/"
        << step << end;
    return false;
  }

  if (length > 0) {
    int64_t minrow = start;
    int64_t maxrow = start + step * (length - 1);
    if (step < 0) std::swap(minrow, maxrow);
    if (min != minrow || max != maxrow) {
      icc << "Invalid min/max values in slice row index " << start << "/"
          << length << "/" << step << ": min = " << min << ", max = "
          << max << end;
    }
  }

  return true;
}
