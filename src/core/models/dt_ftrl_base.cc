//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "models/dt_ftrl_base.h"
#include "parallel/api.h"


namespace dt {


/**
 *  Destructor for the abstract `dt::FtrlBase` class.
 */
FtrlBase::~FtrlBase() {}


/**
 *  Calculate work amount, i.e. number of rows, to be processed
 *  by the zero thread for a MIN_ROWS_PER_THREAD chunk size.
 */
size_t FtrlBase::get_work_amount(size_t nrows) {
  size_t chunk_size = MIN_ROWS_PER_THREAD;
  NThreads nthreads(nthreads_from_niters(nrows, MIN_ROWS_PER_THREAD));
  size_t nth = nthreads.get();

  size_t chunk_rows = chunk_size * (nrows / (nth * chunk_size));
  size_t residual_rows = std::min(nrows - chunk_rows * nth, chunk_size);
  return chunk_rows + residual_rows;
}

} // namespace dt
