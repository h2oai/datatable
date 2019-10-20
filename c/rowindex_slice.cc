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
bool check_slice_triple(size_t start, size_t count, size_t step, size_t max)
{
  // Note: computing `start + step*(count - 1)` may potentially overflow, we
  // must therefore use a safer version.
  return (start <= max) &&
         (count <= RowIndex::MAX) &&
         (count <= 1 || (step == 0) ||
          (step <= RowIndex::MAX? step <= (max - start)/(count - 1)
                                : step >= -start/(count - 1)));
}

static void _check_triple(size_t start, size_t count, size_t step)
{
  if (!check_slice_triple(start, count, step, RowIndex::MAX)) {
    throw ValueError() << "Invalid RowIndex slice [" << start << "/"
                       << count << "/" << step << "]";
  }
}



//------------------------------------------------------------------------------
// SliceRowIndexImpl implementation
//------------------------------------------------------------------------------

SliceRowIndexImpl::SliceRowIndexImpl(size_t i0, size_t n, size_t di) {
  _check_triple(i0, n, di);
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
    arr32_t res(length);
    const int32_t* srcrows = arii->indices32();
    size_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
  }

  if (uptype == RowIndexType::ARR64) {
    auto arii = static_cast<const ArrayRowIndexImpl*>(rii);
    arr64_t res(length);
    const int64_t* srcrows = arii->indices64();
    size_t j = start;
    for (size_t i = 0; i < length; ++i) {
      res[i] = srcrows[j];
      j += step;
    }
    return new ArrayRowIndexImpl(std::move(res), false);
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
    tstep = -tstep;
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
    counts[0] = static_cast<int64_t>(tstart);
    steps[0] = 1;
    starts[1] = static_cast<int64_t>(tstart + tcount);
    counts[1] = static_cast<int64_t>(nrows - (tstart + tcount));
    steps[1] = 1;
    return new ArrayRowIndexImpl(starts, counts, steps);
  }
  size_t j = 0;
  if (nrows <= INT32_MAX) {
    arr32_t indices(newcount);
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
    xassert(j == newcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  } else {
    arr64_t indices(newcount);
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
    xassert(j == newcount);
    return new ArrayRowIndexImpl(std::move(indices), true);
  }
}




size_t SliceRowIndexImpl::memory_footprint() const noexcept {
  return sizeof(*this);
}



void SliceRowIndexImpl::verify_integrity() const {
  RowIndexImpl::verify_integrity();

  if (type != RowIndexType::SLICE) {
    throw AssertionError() << "Invalid type = " << static_cast<int>(type)
        << " in a SliceRowIndex";
  }

  try {
    _check_triple(start, length, step);
  } catch (const Error&) {
    int64_t istep = static_cast<int64_t>(step);
    throw AssertionError()
        << "Invalid slice rowindex: " << start << "/" << length << "/" << istep;
  }

  if (length > 0) {
    size_t minrow = start;
    size_t maxrow = start + step * (length - 1);
    if (!ascending) std::swap(minrow, maxrow);
    if (max != maxrow) {
      int64_t istep = static_cast<int64_t>(step);
      throw AssertionError()
          << "Invalid max value in a Slice RowIndex " << start << "/"
          << length << "/" << istep << ": max = " << max;
    }
    if (ascending != (step <= RowIndex::MAX)) {
      throw AssertionError()
          << "Incorrect 'ascending' flag in Slice RowIndex";
    }
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
