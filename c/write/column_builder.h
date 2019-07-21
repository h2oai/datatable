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
#ifndef dt_WRITE_COLUMN_BUILDER_h
#define dt_WRITE_COLUMN_BUILDER_h
#include "write/output_options.h"
#include "write/value_reader.h"
#include "write/value_writer.h"
namespace dt {
namespace write {


class column_builder {
  private:
    value_reader_ptr reader;
    value_writer_ptr writer;

  public:
    column_builder(const Column*, const output_options&);

    size_t get_static_output_size() const;
    size_t get_dynamic_output_size() const;

    void write_normal(writing_context& ctx, size_t row);
    void write_quoted(writing_context& ctx, size_t row);
};



}}  // namespace dt::write
#endif
