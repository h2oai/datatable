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


void ThreadContext::set_row0(size_t n) {
  xassert(row0_ <= n);
  row0_ = n;
}



//------------------------------------------------------------------------------
// Post-processing
//------------------------------------------------------------------------------

void ThreadContext::preorder() {
  if (!used_nrows) return;
  size_t j = 0;
  for (const auto& col : preframe_) {
    if (!col.is_in_buffer()) continue;
    if (col.is_type_bumped()) { j++; continue; }

    switch (col.get_stype()) {
      case SType::BOOL:    preorder_bool_column(j); break;
      case SType::INT32:   preorder_int32_column(j); break;
      case SType::INT64:   preorder_int64_column(j); break;
      case SType::FLOAT32: preorder_float32_column(j); break;
      case SType::FLOAT64: preorder_float64_column(j); break;
      case SType::STR32:
      case SType::STR64:   preorder_string_column(j); break;
      default: throw RuntimeError() << "Unknown column type";
    }
    ++j;
  }
}


void ThreadContext::preorder_bool_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int8_t>(data->int8);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::preorder_int32_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int32_t>(data->int32);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::preorder_int64_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<int64_t>(data->int64);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::preorder_float32_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<float>(data->float32);
  }
  colinfo_[j].na_count = na_count;
}


void ThreadContext::preorder_float64_column(size_t j) {
  size_t na_count = 0;
  const field64* data = tbuf.data() + j;
  const field64* end = data + used_nrows * tbuf_ncols;
  for (; data < end; data += tbuf_ncols) {
    na_count += ISNA<double>(data->float64);
  }
  colinfo_[j].na_count = na_count;
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
    if (!col.is_in_buffer()) continue;
    if (col.is_type_bumped()) { j++; continue; }

    auto& outcol = col.outcol();
    outcol.add_na_count(colinfo_[j].na_count);
    switch (col.get_stype()) {
      case SType::STR32:
      case SType::STR64:  order_string_column(outcol, j); break;
      default:;
    }
    ++j;
  }
}


void ThreadContext::order_string_column(OutputColumn& col, size_t j) {
  auto strdata_size = colinfo_[j].str.size;
  auto wb = col.strdata_w();
  size_t write_at = wb->prepare_write(strdata_size, nullptr);
  colinfo_[j].str.write_at = write_at;
}




//------------------------------------------------------------------------------
// Post-ordering
//------------------------------------------------------------------------------

void ThreadContext::postorder() {
  // If the buffer is empty, then there's nothing to do...
  if (!used_nrows) return;

  size_t j = 0;
  for (auto& col : preframe_) {
    if (!col.is_in_buffer()) continue;
    if (col.is_type_bumped()) { j++; continue; }

    auto& outcol = col.outcol();
    switch (col.get_stype()) {
      case SType::BOOL:    postorder_bool_column(outcol, j); break;
      case SType::INT32:   postorder_int32_column(outcol, j); break;
      case SType::INT64:   postorder_int64_column(outcol, j); break;
      case SType::FLOAT32: postorder_float32_column(outcol, j); break;
      case SType::FLOAT64: postorder_float64_column(outcol, j); break;
      case SType::STR32:   postorder_string_column(outcol, j); break;
      default:
        throw RuntimeError() << "Unknown column SType in fread";
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
  auto out_data = static_cast<uint32_t*>(col.data_w());

  auto dest = out_data + (row0_ - col.nrows_archived()) + 1;
  for (size_t i = 0; i < used_nrows; ++i) {
    uint32_t offset = src_data->str32.offset;
    int32_t length = src_data->str32.length;
    if (length > 0) {
      auto zlength = static_cast<size_t>(length);
      out_strbuf.write_at(pos0, src_strbuf + offset, zlength);
      pos0 += zlength;
      *dest++ = static_cast<uint32_t>(pos0);
    }
    else {
      static_assert(static_cast<uint32_t>(NA_I4) == NA_S4,
                    "incompatible int32 and uint32 NA values");
      xassert(length == 0 || length == NA_I4);
      *dest++ = static_cast<uint32_t>(pos0) ^ static_cast<uint32_t>(length);
    }
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_bool_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int8_t*>(col.data_w())
                  + (row0_ - col.nrows_archived());
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int8;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_int32_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int32_t*>(col.data_w())
                  + (row0_ - col.nrows_archived());
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int32;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_int64_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<int64_t*>(col.data_w())
                  + (row0_ - col.nrows_archived());
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->int64;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_float32_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<float*>(col.data_w())
                  + (row0_ - col.nrows_archived());
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->float32;
    src_data += tbuf_ncols;
  }
}


void ThreadContext::postorder_float64_column(OutputColumn& col, size_t j) {
  auto src_data = tbuf.data() + j;
  auto out_data = static_cast<double*>(col.data_w())
                  + (row0_ - col.nrows_archived());
  for (size_t i = 0; i < used_nrows; ++i) {
    *out_data++ = src_data->float64;
    src_data += tbuf_ncols;
  }
}




}}  // namespace dt::read
