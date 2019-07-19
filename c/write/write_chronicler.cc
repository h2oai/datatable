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
#include "write/write_chronicler.h"
namespace dt {
namespace write {


write_chronicler::write_chronicler() {}

void write_chronicler::set_logger(py::oobj logger_) {
  logger = std::move(logger_);
}


void write_chronicler::checkpoint_start_writing() {
	t_last = std::chrono::steady_clock::now();
}

void write_chronicler::checkpoint_preamble_done() {
  t_preamble = duration_from_last();
}

void write_chronicler::checkpoint_writing_done() {
  t_writing_rows = duration_from_last();
}

void write_chronicler::checkpoint_the_end() {
  t_epilogue = duration_from_last();
}


void write_chronicler::report_chunking_strategy(
  size_t nrows, size_t nchunks, size_t nthreads, size_t estimated_output_size)
{
  if (!logger) return;
  size_t rows_per_chunk = nrows / nchunks;

  msg() << "File size estimate is " << estimated_output_size << " bytes ";
  msg() << "File will be written using " << nchunks << " chunks, with "
        << rows_per_chunk << " rows per chunk";
  msg() << "Using nthreads = " << nthreads;
}


void write_chronicler::report_final(size_t actual_output_size)
{
  if (!logger) return;
  double t_total = t_preamble + t_writing_rows + t_epilogue;

  msg() << "Final output size is " << actual_output_size << " bytes";
  msg() << "Timing report:";
  msg() << "   " << ff(6, 3, t_preamble)     << "s  Prepare for writing";
  msg() << " + " << ff(6, 3, t_writing_rows) << "s  Write the data";
  msg() << " + " << ff(6, 3, t_epilogue)     << "s  Finalizing";
  msg() << " = " << ff(6, 3, t_total)        << "s  Overall time taken";
}




//------------------------------------------------------------------------------
// Private helpers
//------------------------------------------------------------------------------

double write_chronicler::duration_from_last() {
  auto now = std::chrono::steady_clock::now();
  std::chrono::duration<double> delta = now - t_last;
  t_last = now;
  return delta.count();
}

LogMessage write_chronicler::msg() const {
  return LogMessage(logger);
}


}}  // namespace dt::write
