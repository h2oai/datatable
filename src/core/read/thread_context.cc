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
#include "encodings.h"               // check_escaped_string, decode_escaped_csv_string
#include "py_encodings.h"            // decode_win1252
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


void ThreadContext::postprocess() {
  auto zanchor = reinterpret_cast<const uint8_t*>(parse_ctx_.anchor);
  uint8_t echar = parse_ctx_.quoteRule == 0? static_cast<uint8_t>(parse_ctx_.quote) :
                  parse_ctx_.quoteRule == 1? '\\' : 0xFF;
  uint32_t output_offset = 0;
  for (size_t i = 0, j = 0; i < preframe_.ncols(); ++i) {
    auto& col = preframe_.column(i);
    if (!col.is_in_buffer()) continue;
    if (col.is_string() && !col.is_type_bumped()) {
      strinfo[j].start = output_offset;
      field64* coldata = tbuf.data() + j;
      for (size_t n = 0; n < used_nrows; ++n) {
        // Initially, offsets of all entries are given relative to `zanchor`.
        // If a string is NA, its length will be INT_MIN.
        uint32_t entry_offset = coldata->str32.offset;
        int32_t entry_length = coldata->str32.length;
        if (entry_length > 0) {
          size_t zlen = static_cast<size_t>(entry_length);
          if (sbuf.size() < zlen * 3 + output_offset) {
            sbuf.resize(size_t((2 - 1.0*n/used_nrows)*sbuf.size()) + zlen*3);
          }
          uint8_t* dest = sbuf.data() + output_offset;
          const uint8_t* src = zanchor + entry_offset;
          int res = check_escaped_string(src, zlen, echar);
          int32_t newlen = entry_length;
          if (res == 0) {
            // The most common case: the string is correct UTF-8 and does not
            // require un-escaping. Leave the entry as-is
            std::memcpy(dest, src, zlen);
          } else if (res == 1) {
            // Valid UTF-8, but requires un-escaping
            newlen = decode_escaped_csv_string(src, entry_length, dest, echar);
          } else {
            // Invalid UTF-8
            newlen = decode_win1252(src, entry_length, dest);
            xassert(newlen > 0);
            newlen = decode_escaped_csv_string(dest, newlen, dest, echar);
          }
          xassert(newlen > 0);
          output_offset += static_cast<uint32_t>(newlen);
          coldata->str32.length = newlen;
          coldata->str32.offset = output_offset;
        } else if (entry_length == 0) {
          coldata->str32.offset = output_offset;
        } else {
          xassert(coldata->str32.isna());
          coldata->str32.offset = output_offset ^ GETNA<uint32_t>();
        }
        coldata += tbuf_ncols;
        xassert(output_offset <= sbuf.size());
      }
    }
    ++j;
  }
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
      size_t write_at = wb->prepare_write(sz, sbuf.data() + offset0);
      strinfo[j].write_at = write_at;
    }
    ++j;
  }
}




void ThreadContext::push_buffers() {
  // If the buffer is empty, then there's nothing to do...
  if (!used_nrows) return;

  size_t j = 0;
  for (auto& col : preframe_) {
    if (!col.is_in_buffer()) continue;
    auto& outcol = col.outcol();
    void* data = outcol.data_w();
    int8_t elemsize = static_cast<int8_t>(col.elemsize());
    size_t effective_row0 = row0 - col.nrows_archived();

    if (col.is_type_bumped()) {
      // do nothing: the column was not properly allocated for its type, so
      // any attempt to write the data may fail with data corruption
    } else if (col.is_string()) {
      WritableBuffer* wb = outcol.strdata_w();
      auto& si = strinfo[j];
      field64* lo = tbuf.data() + j;

      wb->write_at(si.write_at, si.size, sbuf.data() + si.start);

      if (elemsize == 4) {
        uint32_t* dest = static_cast<uint32_t*>(data) + effective_row0 + 1;
        uint32_t delta = static_cast<uint32_t>(si.write_at - si.start);
        for (size_t n = 0; n < used_nrows; ++n) {
          uint32_t soff = lo->str32.offset;
          *dest++ = soff + delta;
          lo += tbuf_ncols;
        }
      } else {
        uint64_t* dest = static_cast<uint64_t*>(data) + effective_row0 + 1;
        uint64_t delta = static_cast<uint64_t>(si.write_at - si.start);
        for (size_t n = 0; n < used_nrows; ++n) {
          uint64_t soff = lo->str32.offset;
          *dest++ = soff + delta;
          lo += tbuf_ncols;
        }
      }

    } else {
      const field64* src = tbuf.data() + j;
      if (elemsize == 8) {
        uint64_t* dest = static_cast<uint64_t*>(data) + effective_row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint64;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 4) {
        uint32_t* dest = static_cast<uint32_t*>(data) + effective_row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint32;
          src += tbuf_ncols;
          dest++;
        }
      } else
      if (elemsize == 1) {
        uint8_t* dest = static_cast<uint8_t*>(data) + effective_row0;
        for (size_t r = 0; r < used_nrows; r++) {
          *dest = src->uint8;
          src += tbuf_ncols;
          dest++;
        }
      }
    }
    j++;
  }
  used_nrows = 0;
}




}}  // namespace dt::read
