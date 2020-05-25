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
    colinfo_(ncols),
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



//------------------------------------------------------------------------------
// Post-processing
//------------------------------------------------------------------------------

void ThreadContext::postprocess() {
  size_t j = 0;
  for (const auto& col : preframe_) {
    if (!col.is_in_buffer()) continue;
    if (!col.is_type_bumped()) {
      switch (col.get_stype()) {
        case SType::BOOL:    postprocess_bool_column(j); break;
        case SType::INT32:   postprocess_int32_column(j); break;
        case SType::INT64:   postprocess_int64_column(j); break;
        case SType::FLOAT32: postprocess_float32_column(j); break;
        case SType::FLOAT64: postprocess_float64_column(j); break;
        case SType::STR32:
        case SType::STR64:   postprocess_string_column(j); break;
        default:;
      }
    }
    ++j;
  }
}


void ThreadContext::postprocess_bool_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int8_t>(data->int8);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::postprocess_int32_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int32_t>(data->int32);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::postprocess_int64_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int64_t>(data->int64);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::postprocess_float32_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<float>(data->float32);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::postprocess_float64_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<double>(data->float64);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::postprocess_string_column(size_t j) {
  size_t total_length = 0;
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    int32_t len = data->str32.length;
    if (len > 0) {
      total_length += static_cast<size_t>(len);
    }
    else if (len < 0) {
      na_count++;
    }
  }
  colinfo_[j].str.size = total_length;
  colinfo_[j].na_count = na_count;
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
      // uint32_t offset0 = static_cast<uint32_t>(colinfo_[j].str.start);
      // uint32_t offsetL = tbuf[j + tbuf_ncols * (used_nrows - 1)].str32.offset;
      // size_t sz = (offsetL - offset0) & ~GETNA<uint32_t>();
      // colinfo_[j].str.size = sz;

      auto sz = colinfo_[j].str.size;
      auto wb = outcol.strdata_w();
      size_t write_at = wb->prepare_write(sz, nullptr);
      colinfo_[j].str.write_at = write_at;
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
      auto srcbuf = static_cast<char*>(parse_ctx_.strbuf.data());
      auto outbuf = outcol.strdata_w();
      field64* dataptr = tbuf.data() + j;

      auto pos0 = static_cast<uint32_t>(colinfo_[j].str.write_at);
      auto dest = static_cast<uint32_t*>(data) + effective_row0 + 1;
      xassert(elemsize == 4);
      for (size_t n = 0; n < used_nrows; ++n) {
        uint32_t offset = dataptr->str32.offset;
        int32_t length = dataptr->str32.length;
        if (length > 0) {
          outbuf->write_at(pos0, static_cast<size_t>(length), srcbuf + offset);
          pos0 += static_cast<uint32_t>(length);
          *dest++ = pos0;
        }
        else {
          static_assert(static_cast<uint32_t>(NA_I4) == NA_S4,
                        "incompatible int32 and uint32 NA values");
          xassert(length == 0 || length == NA_I4);
          *dest++ = pos0 ^ static_cast<uint32_t>(length);
        }
        dataptr += tbuf_ncols;
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
