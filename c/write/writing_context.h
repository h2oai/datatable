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
#ifndef dt_WRITE_WRITING_CONTEXT_h
#define dt_WRITE_WRITING_CONTEXT_h
#include <memory>     // std::unique_ptr
#include "types.h"
namespace dt {
namespace write {


class zlib_writer;


class writing_context {
  public:
    char* ch;   // current writing position

    union {
      int32_t value_i32;
      int64_t value_i64;
      float   value_f32;
      double  value_f64;
      CString value_str;
    };

  private:
    CString output;
    // do not write var-width fields past this pointer, need to reallocate
    char* end;
    char* buffer;
    size_t buffer_capacity;
    size_t fixed_size_per_row;

    // Either nullptr if no compression is needed, or an instance of zlib_writer
    // class, defined in "writer/zlib_writer.h"
    zlib_writer* zwriter;

  public:
    writing_context(size_t size_per_row, size_t nrows, bool compress = false);
    ~writing_context();

    void ensure_buffer_capacity(size_t sz);
    void finalize_buffer();
    CString get_buffer() const;
    void reset_buffer();

    void write_na() {}

  private:
    void allocate_buffer(size_t sz);
};



}}  // namespace dt::write
#endif
