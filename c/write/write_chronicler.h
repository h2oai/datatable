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
#ifndef dt_WRITE_WRITE_CHRONICLER_h
#define dt_WRITE_WRITE_CHRONICLER_h
#include <chrono>
#include "python/obj.h"
#include "utils/logger.h"
namespace dt {
namespace write {


class write_chronicler {
  private:
    using ptime_t = std::chrono::steady_clock::time_point;

    py::oobj logger;
    ptime_t t_last;
    double t_preamble;
    double t_writing_rows;
    double t_epilogue;

  public:
    write_chronicler();
    void set_logger(py::oobj logger_);

    void checkpoint_start_writing();
    void checkpoint_preamble_done();
    void checkpoint_writing_done();
    void checkpoint_the_end();

    void report_chunking_strategy(size_t nrows, size_t nchunks, size_t nthreads,
                                  size_t estimated_output_size);
    void report_final(size_t actual_output_size);

  private:
    double duration_from_last();
    LogMessage msg() const;
};



}}  // namespace dt::write
#endif
