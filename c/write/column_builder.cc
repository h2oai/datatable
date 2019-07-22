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
#include "write/column_builder.h"
namespace dt {
namespace write {


column_builder::column_builder(const Column* col, const output_options& options)
  : reader(value_reader::create(col)),
    writer(value_writer::create(col->stype(), options)) {}


size_t column_builder::get_static_output_size() const {
  return writer->get_static_output_size();
}

size_t column_builder::get_dynamic_output_size() const {
  return writer->get_dynamic_output_size();
}


void column_builder::write_normal(writing_context& ctx, size_t row) {
  bool isvalid = reader->read(ctx, row);
  if (isvalid) {
    writer->write(ctx);
  } else {
    ctx.write_na();
  }
}


void column_builder::write_quoted(writing_context& ctx, size_t row) {
  bool isvalid = reader->read(ctx, row);
  if (isvalid) {
    *ctx.ch++ = '"';
    writer->write(ctx);
    *ctx.ch++ = '"';
  } else {
    ctx.write_na();
  }
}



}}  // namespace dt::write
