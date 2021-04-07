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
#include <limits>
#include "read/field64.h"            // field64
#include "read/chunk_coordinates.h"  // ChunkCoordinates
#include "read/preframe.h"
#include "read/thread_context.h"
#include "utils/assert.h"
#include "encodings.h"               // check_escaped_string, decode_escaped_csv_string
#include "py_encodings.h"            // decode_win1252
#include "stype.h"
namespace dt {
namespace read {


ThreadContext::ThreadContext(size_t ncols, size_t nrows, PreFrame& preframe)
  : tbuf(ncols * nrows + 1),
    colinfo_(ncols),
    tbuf_ncols(ncols),
    tbuf_nrows(nrows),
    used_nrows(0),
    row0_(0),
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


void ThreadContext::set_row0() {
  size_t n = preframe_.nrows_written();
  xassert(row0_ <= n);
  row0_ = n;
}


size_t ThreadContext::ensure_output_nrows(
    size_t chunk_nrows, size_t ichunk, dt::OrderedTask* otask)
{
  return preframe_.ensure_output_nrows(chunk_nrows, ichunk, otask);
}



//------------------------------------------------------------------------------
// Post-processing
//------------------------------------------------------------------------------

void ThreadContext::preorder() {
  if (!used_nrows) return;
  size_t j = 0;
  for (const auto& col : preframe_) {
    switch (col.get_stype()) {
      case SType::VOID:    break;
      case SType::BOOL:    preorder_bool_column(j); break;
      case SType::DATE32:
      case SType::INT32:   preorder_int32_column(j); break;
      case SType::TIME64:
      case SType::INT64:   preorder_int64_column(j); break;
      case SType::FLOAT32: preorder_float32_column(j); break;
      case SType::FLOAT64: preorder_float64_column(j); break;
      case SType::STR32:
      case SType::STR64:   preorder_string_column(j); break;
      default: throw RuntimeError()
        << "Unknown column type in ThreadContext::preorder()";
    }
    ++j;
  }
}


void ThreadContext::preorder_bool_column(size_t j) {
  size_t count0 = 0;
  size_t count1 = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    count0 += (data->int8 == 0);
    count1 += (data->int8 == 1);
  }
  colinfo_[j].na_count = used_nrows - count0 - count1;
  colinfo_[j].b.count0 = count0;
  colinfo_[j].b.count1 = count1;
}


void ThreadContext::preorder_int32_column(size_t j) {
  size_t na_count = 0;
  int32_t min = std::numeric_limits<int32_t>::max();
  int32_t max = -min;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    auto x = data->int32;
    if (ISNA<int32_t>(x)) {
      na_count++;
    } else {
      if (x < min) min = x;
      if (x > max) max = x;
    }
  }
  colinfo_[j].na_count = na_count;
  colinfo_[j].i.min = min;
  colinfo_[j].i.max = max;
}


void ThreadContext::preorder_int64_column(size_t j) {
  size_t na_count = 0;
  int64_t min = std::numeric_limits<int64_t>::max();
  int64_t max = -min;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    auto x = data->int64;
    if (ISNA<int64_t>(x)) {
      na_count++;
    } else {
      if (x < min) min = x;
      if (x > max) max = x;
    }
  }
  colinfo_[j].na_count = na_count;
  colinfo_[j].i.min = min;
  colinfo_[j].i.max = max;
}


void ThreadContext::preorder_float32_column(size_t j) {
  size_t na_count = 0;
  float min = std::numeric_limits<float>::infinity();
  float max = -min;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    auto x = data->float32;
    if (ISNA<float>(x)) {
      na_count++;
    } else {
      if (x < min) min = x;
      if (x > max) max = x;
    }
  }
  colinfo_[j].na_count = na_count;
  colinfo_[j].f.min = static_cast<double>(min);
  colinfo_[j].f.max = static_cast<double>(max);
}


void ThreadContext::preorder_float64_column(size_t j) {
  size_t na_count = 0;
  double min = std::numeric_limits<double>::infinity();
  double max = -min;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    auto x = data->float64;
    if (ISNA<double>(x)) {
      na_count++;
    } else {
      if (x < min) min = x;
      if (x > max) max = x;
    }
  }
  colinfo_[j].na_count = na_count;
  colinfo_[j].f.min = min;
  colinfo_[j].f.max = max;
}


void ThreadContext::preorder_string_column(size_t j) {
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




//------------------------------------------------------------------------------
// Ordering
//------------------------------------------------------------------------------

void ThreadContext::order() {
  if (!used_nrows) return;
  size_t j = 0;
  for (auto& col : preframe_) {
    auto& outcol = col.outcol();
    outcol.merge_chunk_stats(colinfo_[j]);
    if (col.is_string()) {
      auto strdata_size = colinfo_[j].str.size;
      auto wb = outcol.strdata_w();
      size_t write_at = wb->prepare_write(strdata_size, nullptr);
      colinfo_[j].str.write_at = write_at;
    }
    ++j;
  }
}




//------------------------------------------------------------------------------
// Post-ordering
//------------------------------------------------------------------------------

void ThreadContext::postorder() {
  // If the buffer is empty, then there's nothing to do...
  if (!used_nrows) return;

  size_t j = 0;
  for (auto& col : preframe_) {
    auto& outcol = col.outcol();
    switch (col.get_stype()) {
      case SType::VOID:    break;
      case SType::BOOL:    postorder_bool_column(outcol, j); break;
      case SType::DATE32:
      case SType::INT32:   postorder_int32_column(outcol, j); break;
      case SType::TIME64:
      case SType::INT64:   postorder_int64_column(outcol, j); break;
      case SType::FLOAT32: postorder_float32_column(outcol, j); break;
      case SType::FLOAT64: postorder_float64_column(outcol, j); break;
      case SType::STR32:   postorder_string_column(outcol, j); break;
      default:
        throw RuntimeError() << "Unknown column of type "
            << col.get_stype() << " in fread";
    }
    j++;
  }
  used_nrows = 0;
}


void ThreadContext::postorder_string_column(OutputColumn& col, size_t j) {
  // Note: `out_strbuf` is a Writer object, which holds a non-exclusive
  //       lock on the underlying buffer for the duration of its lifetime.
  auto pos0 = colinfo_[j].str.write_at;
  auto len0 = colinfo_[j].str.size;
  auto src_strbuf = static_cast<const char*>(parse_ctx_.strbuf.rptr());
  auto out_strbuf = col.strdata_w()->writer(pos0, pos0 + len0);
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<uint32_t*>(col.data_w(row0_ + 1));

  for (size_t i = 0; i < used_nrows; ++i) {
    uint32_t offset = src_data->str32.offset;
    int32_t length = src_data->str32.length;
    if (length > 0) {
      auto zlength = static_cast<size_t>(length);
      out_strbuf.write_at(pos0, src_strbuf + offset, zlength);
      pos0 += zlength;
      *out_data++ = static_cast<uint32_t>(pos0);
    }
    else {
      static_assert(static_cast<uint32_t>(NA_I4) == NA_S4,
                    "incompatible int32 and uint32 NA values");
      xassert(length == 0 || length == NA_I4);
      *out_data++ = static_cast<uint32_t>(pos0) ^ static_cast<uint32_t>(length);
    }
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_bool_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int8_t*>(col.data_w(row0_));
  xassert(src_data && out_data);
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int8;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_int32_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int32_t*>(col.data_w(row0_));
  xassert(src_data && out_data);
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int32;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_int64_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int64_t*>(col.data_w(row0_));
  xassert(src_data && out_data);
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int64;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_float32_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<float*>(col.data_w(row0_));
  xassert(src_data && out_data);
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->float32;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_float64_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<double*>(col.data_w(row0_));
  xassert(src_data && out_data);
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->float64;
    src_data += tbuf_ncols;
  }
}




}}  // namespace dt::read
