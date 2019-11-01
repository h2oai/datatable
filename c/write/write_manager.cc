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
#include "parallel/api.h"          // parallel_for_ordered
#include "progress/work.h"         // progress::work
#include "python/string.h"         // py::ostring
#include "utils/assert.h"          // xassert
#include "utils/misc.h"            // nlz
#include "write/write_manager.h"
namespace dt {
namespace write {


//------------------------------------------------------------------------------
// Construction
//------------------------------------------------------------------------------

write_manager::write_manager(DataTable* dt_, std::string path_)
  : dt(dt_),
    path(std::move(path_)),
    strategy(WritableBuffer::Strategy::Auto),
    fixed_size_per_row(0),
    estimated_output_size(0),
    nchunks(0) {}

write_manager::~write_manager() {}


//---- Deferred initialization of certain parameters ----

void write_manager::set_append(bool f) {
  append_ = f;
}

void write_manager::set_header(bool f) {
  write_header_ = f;
}

void write_manager::set_strategy(WritableBuffer::Strategy strategy_) {
  strategy = strategy_;
}

void write_manager::set_logger(py::oobj logger) {
  chronicler.set_logger(std::move(logger));
}

void write_manager::set_usehex(bool f) {
  options.floats_as_hex = f;
  options.integers_as_hex = f;
}

void write_manager::set_quoting(int q) {
  options.quoting_mode = static_cast<Quoting>(q);
}

void write_manager::set_compression(bool f) {
  options.compress_zlib = f;
}



//------------------------------------------------------------------------------
// Main writing method
//------------------------------------------------------------------------------

void write_manager::write_main()
{
  chronicler.checkpoint_start_writing();
  progress::work job(WRITE_PREPARE + WRITE_MAIN + WRITE_FINALIZE);
  job.set_message(get_job_name());

  create_column_writers();
  estimate_output_size();
  create_output_target();
  write_preamble();
  determine_chunking_strategy();

  chronicler.checkpoint_preamble_done();
  job.add_done_amount(WRITE_PREPARE);

  if (dt->nrows() > 0 && dt->ncols() > 0) {
    job.add_tentative_amount(WRITE_MAIN);
    write_rows();
  }

  job.add_done_amount(WRITE_MAIN);
  chronicler.checkpoint_writing_done();

  write_epilogue();
  finalize_output();
  job.add_done_amount(WRITE_FINALIZE);
  job.done();

  chronicler.checkpoint_the_end();
  chronicler.report_final(wb->size());
}


void write_manager::write_rows()
{
  // Check that `nchunks * nrows` doesn't overflow
  size_t nrows = dt->nrows();
  xassert(nchunks <= size_t(-1) / nrows);

  parallel_for_ordered(
    nchunks,  // number of iterations
    [&](ordered* o) {
      size_t nrows_per_chunk =  dt->nrows() / nchunks;
      writing_context ctx(fixed_size_per_row, nrows_per_chunk,
                          options.compress_zlib);
      size_t th_write_at = 0;
      size_t th_write_size = 0;

      o->parallel(
        [&](size_t i) {  // pre-ordered
          size_t row0 = i * nrows / nchunks;
          size_t row1 = (i + 1) * nrows / nchunks;
          for (size_t row = row0; row < row1; ++row) {
            write_row(ctx, row);
          }
          ctx.finalize_buffer();
        }, // end of pre-ordered

        [&](size_t) {  // ordered
          CString buf = ctx.get_buffer();
          th_write_size = static_cast<size_t>(buf.size);
          th_write_at = wb->prep_write(th_write_size, buf.ch);
        },

        [&](size_t) {  // post-ordered
          CString buf = ctx.get_buffer();
          wb->write_at(th_write_at, th_write_size, buf.ch);
          th_write_size = 0;
          ctx.reset_buffer();
        }
      );
    });
}


py::oobj write_manager::get_result() {
  xassert(result);
  return result;
}




//------------------------------------------------------------------------------
// Helper steps
//------------------------------------------------------------------------------

void write_manager::create_column_writers() {
  for (size_t i = 0; i < dt->ncols(); ++i) {
    const Column& col = dt->get_column(i);
    columns.push_back(value_writer::create(col, options));
  }
}


void write_manager::create_output_target() {
  wb = WritableBuffer::create_target(path, estimated_output_size, strategy,
                                     append_);
}


/**
 * Compute parameters for writing the file: how many chunks to use, how many
 * rows per chunk, etc.
 *
 * This function depends only on parameters `bytes_total`, `nrows` and
 * `nthreads`. Its effect is to fill in values `rows_per_chunk`, 'nchunks' and
 * `bytes_per_chunk`.
 */
void write_manager::determine_chunking_strategy()
{
  size_t nrows = dt->nrows();
  if (nrows == 0 || dt->ncols() == 0) return;
  xassert(estimated_output_size > 0)
  const double bytes_per_row = 1.0 * estimated_output_size / nrows;

  static constexpr size_t max_chunk_size = 1024 * 1024;
  static constexpr size_t min_chunk_size = 1024;

  size_t nthreads = dt::num_threads_in_pool();
  size_t min_nchunks_for_threadpool = (nthreads == 1) ? 1 : nthreads*2;

  nchunks = std::max(1 + (estimated_output_size - 1) / max_chunk_size,
                     min_nchunks_for_threadpool);
  xassert(nchunks > 0);

  int attempts = 5;
  while (attempts--) {
    double rows_per_chunk = 1.0 * (nrows + 1) / nchunks;
    auto bytes_per_chunk = static_cast<size_t>(bytes_per_row * rows_per_chunk);
    if (rows_per_chunk < 1.0) {
      // If each row's size is too large, then parse 1 row at a time.
      nchunks = nrows;
    } else if (bytes_per_chunk < min_chunk_size && nchunks > 1) {
      // The data is too small, and number of available threads too large --
      // reduce the number of chunks so that we don't waste resources on
      // needless thread manipulation.
      // This formula guarantees that new bytes_per_chunk will be no less
      // than min_chunk_size (or nchunks will be 1).
      nchunks = estimated_output_size / min_chunk_size;
      if (nchunks < 1) nchunks = 1;
    } else {
      chronicler.report_chunking_strategy(nrows, nchunks, nthreads,
                                          estimated_output_size);
      return;
    }
  }
  // This shouldn't really happen, but who knows...
  throw RuntimeError() << "Unable to determine how to write the file"
                       << ": estimated_output_size = " << estimated_output_size
                       << ", nrows = " << nrows
                       << ", nthreads = " << nthreads;  // LCOV_EXCL_LINE
}



void write_manager::finalize_output()
{
  if (path.empty()) {
    // When writing to stdout, append '\0' to make it look like a regular
    // C string, then convert the output buffer `wb` into a pyobject and
    // save in the `result`.
    size_t len = wb->size();
    char c = '\0';
    wb->write(1, &c);
    wb->finalize();

    MemoryWritableBuffer* mb = dynamic_cast<MemoryWritableBuffer*>(wb.get());
    xassert(mb);
    Buffer buf = mb->get_mbuf();
    auto str = static_cast<const char*>(buf.rptr());
    if (options.compress_zlib) {
      auto size = static_cast<Py_ssize_t>(len);
      PyObject* obj = PyBytes_FromStringAndSize(str, size);
      if (!obj) throw PyError();
      result = py::oobj::from_new_reference(obj);
    } else {
      result = py::ostring(str, len);
    }
  }
  else {
    // When writing to a file, all we need is to finalize the buffer
    // (this closes the file handle), and store py::None as the result.
    wb->finalize();
    result = py::None();
  }
}



}}  // namespace dt::write
