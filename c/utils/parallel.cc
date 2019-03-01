//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "utils/parallel.h"
#include "options.h"
#include "utils/exceptions.h"

namespace dt {


//------------------------------------------------------------------------------
// Interleaved
//------------------------------------------------------------------------------

void run_parallel(rangefn run, size_t nrows)
{
  // `min_nrows_per_thread`: avoid processing less than this many rows in each
  // thread, reduce the number of threads if necessary.
  // `min_nrows_per_batch`: the minimum number of rows to process within each
  // thread before sending a progress report signal and checking for
  // interrupts.
  //
  constexpr size_t min_nrows_per_thread = 100;
  constexpr size_t min_nrows_per_batch = 10000;

  if (nrows < min_nrows_per_thread) {
    run(0, nrows, 1);
    // progress.report(nrows);
  }
  else {
    // If the number of rows is too small, then we want to reduce the number of
    // processing threads.
    int nth0 = std::min(config::nthreads,
                        static_cast<int>(nrows / min_nrows_per_thread));
    xassert(nth0 > 0);
    OmpExceptionManager oem;
    #pragma omp parallel num_threads(nth0)
    {
      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());
      size_t batchsize = min_nrows_per_batch * nth;
      try {
        size_t i = ith;
        do {
          size_t iend = std::min(i + batchsize, nrows);
          run(i, iend, nth);
          i = iend;
          // if (ith == 0) progress.report(iend);
        } while (i < nrows && !oem.stop_requested());
      } catch (...) {
        oem.capture_exception();
      }
    }
    oem.rethrow_exception_if_any();
  }
}



//------------------------------------------------------------------------------
// ordered_job
//------------------------------------------------------------------------------

ordered_job::ordered_job(size_t n) : nrows(n) {}

ordered_job::~ordered_job() {}


void ordered_job::execute()
{
  constexpr size_t min_nrows_per_thread = 100;
  size_t nth0 = std::min(static_cast<size_t>(config::nthreads),
                         nrows / min_nrows_per_thread);

  if (nth0 <= 1) {
    ojcptr ctx = make_thread_context();
    run(ctx, 0, nrows);
    order(ctx);
    run(ctx, nrows, nrows);
    // progress.report(nrows);
  }
  else {
    OmpExceptionManager oem;
    #pragma omp parallel num_threads(nth0)
    {
      // int ith = omp_get_thread_num();
      // int nth = omp_get_num_threads();
      size_t nchunks = 1 + (nrows - 1)/1000;
      size_t chunksize = 1 + (nrows - 1)/nchunks;
      ojcptr ctx;

      try {
        ctx = make_thread_context();
      } catch (...) {
        oem.capture_exception();
      }

      #pragma omp for ordered schedule(dynamic)
      for (size_t j = 0; j < nchunks; ++j) {
        if (oem.stop_requested()) continue;
        size_t i0 = j * chunksize;
        size_t i1 = std::min(i0 + chunksize, nrows);
        try {
          run(ctx, i0, i1);
          // if (ith == 0) progress.report(i1);
        } catch (...) {
          oem.capture_exception();
        }
        #pragma omp ordered
        {
          try {
            order(ctx);
          } catch (...) {
            oem.capture_exception();
          }
        }
      }

      try {
        run(ctx, nrows, nrows);
      } catch (...) {
        oem.capture_exception();
      }
    }
    oem.rethrow_exception_if_any();
  }
}


}  // namespace dt
