//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "utils/alloc.h"
#include "utils/assert.h"
#include "write/writing_context.h"
#include "write/zlib_writer.h"
namespace dt {
namespace write {



writing_context::writing_context(
  size_t size_per_row, size_t nrows, bool compress)
{
  fixed_size_per_row = size_per_row;
  ch = nullptr;
  end = nullptr;
  buffer = nullptr;
  buffer_capacity = 0;
  zwriter = compress? new zlib_writer : nullptr;
  allocate_buffer(size_per_row * nrows * 2);
}

writing_context::~writing_context() {
  dt::free(buffer);
  delete zwriter;
}


void writing_context::ensure_buffer_capacity(size_t sz) {
  if (ch + sz >= end) {
    allocate_buffer((sz + buffer_capacity) * 2);
    xassert(ch + sz < end);
  }
}


void writing_context::finalize_buffer() {
  output.ch = buffer;
  output.size = ch - buffer;
  if (zwriter) {
    zwriter->compress(output);  // updates `output` variable
  }
}


void writing_context::reset_buffer() {
  ch = buffer;
  output.ch = nullptr;
}


CString writing_context::get_buffer() const {
  xassert(output.ch);
  return output;
}


void writing_context::allocate_buffer(size_t sz) {
  auto offset_from_start = ch - buffer;
  buffer = static_cast<char*>(dt::_realloc(buffer, sz));
  buffer_capacity = sz;
  ch = buffer + offset_from_start;
  end = buffer + sz - fixed_size_per_row;
}



}}  // namespace dt::write
