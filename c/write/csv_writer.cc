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
#include <memory>                // std::unique_ptr
#include "column/strvec.h"
#include "write/csv_writer.h"
#include "write/value_writer.h"
#include "write/writing_context.h"
namespace dt {
namespace write {


std::string csv_writer::get_job_name() const {
  return "Writing CSV";
}



/**
 * Estimate the expected size of the output and save it in the
 * `estimated_output_size` runtime variable; also compute the
 * `fixed_size_per_row`.
 *
 * The `fixed_size_per_row` contains an upper bound of the output
 * size taking into account only "static" columns (i.e. those where
 * such upper bound exists). Thus, this quantity overestimates the
 * final size, perhaps even significantly.
 *
 * The `rowsize_dynamic` computes a guess for the total output size
 * from all "non-static" columns, such as strings. This is a wild
 * guess: it is impossible to know the dynamic size in advance,
 * especially if the column's content is generated on the fly. The
 * only hope for this variable is that it will be a better estimate
 * than zero.
 *
 */
void csv_writer::estimate_output_size() {
  size_t nrows = dt->nrows();
  size_t ncols = dt->ncols();
  const strvec& column_names = dt->get_names();

  size_t total_columns_size = 0;
  size_t rowsize_fixed = 0;
  size_t rowsize_dynamic = 0;

  for (size_t i = 0; i < ncols; ++i) {
    rowsize_fixed += columns[i]->get_static_output_size();
    rowsize_dynamic += columns[i]->get_dynamic_output_size();
    total_columns_size += column_names[i].size() + 1;
  }
  rowsize_fixed += 1 * ncols;  // separators

  fixed_size_per_row = rowsize_fixed;
  estimated_output_size = (rowsize_fixed + rowsize_dynamic) * nrows
                          + total_columns_size;
}



/**
 * Write the first row of column names
 */
void csv_writer::write_preamble() {
  const strvec& column_names = dt->get_names();
  if (column_names.empty()) return;
  if (!write_header_) return;

  Column names_as_col = Column(new Strvec_ColumnImpl(column_names));
  auto writer = value_writer::create(names_as_col, options);
  writing_context ctx { 3*dt->ncols(), 1, options.compress_zlib };

  if (options.quoting_mode == Quoting::ALL) {
    for (size_t i = 0; i < dt->ncols(); ++i) {
      writer->write_quoted(i, ctx);
      *ctx.ch++ = ',';
    }
  } else {
    for (size_t i = 0; i < dt->ncols(); ++i) {
      writer->write_normal(i, ctx);
      *ctx.ch++ = ',';
    }
  }
  // Replace the last ',' with a newline. This is valid since `ncols > 0`.
  ctx.ch[-1] = '\n';

  // Write this string buffer into the target.
  ctx.finalize_buffer();
  wb->write(ctx.get_buffer());
}



void csv_writer::write_row(writing_context& ctx, size_t j) {
  if (options.quoting_mode == Quoting::ALL) {
    for (const auto& writer : columns) {
      writer->write_quoted(j, ctx);
      *ctx.ch++ = ',';
    }
  } else {
    for (const auto& writer : columns) {
      writer->write_normal(j, ctx);
      *ctx.ch++ = ',';
    }
  }
  ctx.ch[-1] = '\n';
}




}}  // namespace dt::write
