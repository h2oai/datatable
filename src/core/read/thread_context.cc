//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "read/field64.h"            // field64
#include "read/chunk_coordinates.h"  // ChunkCoordinates
#include "read/preframe.h"
#include "read/thread_context.h"
#include "utils/assert.h"
namespace dt {
namespace read {


ThreadContext::ThreadContext(size_t ncols, size_t nrows, PreFrame& preframe)
  : tbuf(ncols * nrows + 1),
    sbuf(0),
    strinfo(ncols),
    tbuf_ncols(ncols),
    tbuf_nrows(nrows),
    used_nrows(0),
    row0(0),
    preframe_(preframe) {}


// Note: it is possible to have `used_nrows != 0` at the end: the content of
// the buffer may be left un-pushed if an exception occurred, or the iterations
// stopped early for some other reason.
ThreadContext::~ThreadContext() {}


void ThreadContext::allocate_tbuf(size_t ncols, size_t nrows) {
  tbuf.resize(ncols * nrows + 1);
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
}


size_t ThreadContext::get_nrows() const {
  return used_nrows;
}


void ThreadContext::set_nrows(size_t n) {
  xassert(n <= used_nrows);
  used_nrows = n;
}


void ThreadContext::set_row0(size_t n) {
  xassert(row0 <= n);
  row0 = n;
}


void ThreadContext::order_buffer() {
  if (!used_nrows) return;
  size_t j = 0;
  for (auto& col : preframe_) {
    if (!col.is_in_buffer()) continue;
    if (col.is_string() && !col.is_type_bumped()) {
      auto& outcol = col.outcol();
      // Compute the size of the string content in the buffer `sz` from the
      // offset of the last element. This quantity cannot be calculated in the
      // postprocess() step, since `used_nrows` may sometimes change, affecting
      // this size after the post-processing.
      uint32_t offset0 = static_cast<uint32_t>(strinfo[j].start);
      uint32_t offsetL = tbuf[j + tbuf_ncols * (used_nrows - 1)].str32.offset;
      size_t sz = (offsetL - offset0) & ~GETNA<uint32_t>();
      strinfo[j].size = sz;

      WritableBuffer* wb = outcol.strdata_w();
      size_t write_at = wb->prep_write(sz, sbuf.data() + offset0);
      strinfo[j].write_at = write_at;
    }
    ++j;
  }
}




}}  // namespace dt::read
