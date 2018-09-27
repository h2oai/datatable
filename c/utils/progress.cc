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
#include "utils/progress.h"
#include "options.h"
#include "utils/exceptions.h"
#include "utils/omp.h"

namespace dt {


//------------------------------------------------------------------------------
// Interleaved
//------------------------------------------------------------------------------

void run_interleaved(size_t nrows,
                     std::function<void(size_t&, size_t, size_t)> run)
{
  // If the number of rows is too small, then we want to reduce the number of
  // processing threads, or even fall back to single-threaded execution.
  constexpr size_t min_nrows_per_thread = 100;
  size_t nth0 = std::min(static_cast<size_t>(config::nthreads),
                         nrows / min_nrows_per_thread);

  if (nth0 <= 1) {
    size_t i = 0;
    run(i, nrows, 1);
    // progress.report(nrows);
  }
  else {
    OmpExceptionManager oem;
    #pragma omp parallel num_threads(nth0)
    {
      try {
        size_t ith = static_cast<size_t>(omp_get_thread_num());
        size_t nth = static_cast<size_t>(omp_get_num_threads());
        if (ith == 0) {
          size_t chunksize = 100000;  // std::max(progress.total_effort()/100, 100000);
          size_t i = ith;
          size_t iend = std::min(nrows, chunksize);
          do {
            run(i, iend, nth);
            // progress.report(iend);
            iend += chunksize;
            if (iend > nrows) iend = nrows;
          } while (i < nrows);
        }
        else {
          run(ith, nrows, nth);
        }
      } catch (...) {
        oem.capture_exception();
      }
    }
    oem.rethrow_exception_if_any();
  }
}



//------------------------------------------------------------------------------
// Ordered
//------------------------------------------------------------------------------

void run_ordered(size_t nrows, ojcptr (*prepare)(int, int))
{
  constexpr size_t min_nrows_per_thread = 100;
  size_t nth0 = std::min(static_cast<size_t>(config::nthreads),
                         nrows / min_nrows_per_thread);

  if (nth0 <= 1) {
    ojcptr ctx = prepare(0, 1);
    ctx->run(0, nrows);
    ctx->commit();
    // progress.report(nrows);
  }
  else {
    OmpExceptionManager oem;
    bool stop_iteration = false;
    #pragma omp parallel num_threads(nth0)
    {
      int ith = omp_get_thread_num();
      int nth = omp_get_num_threads();
      size_t nchunks = 1 + (nrows - 1)/1000;
      size_t chunksize = 1 + (nrows - 1)/nchunks;
      ojcptr ctx;

      try {
        ctx = (*prepare)(ith, nth);
      } catch (...) {
        oem.capture_exception();
        stop_iteration = true;
      }

      #pragma omp for ordered schedule(dynamic)
      for (size_t j = 0; j < nchunks; ++j) {
        if (stop_iteration) continue;
        size_t i0 = j * chunksize;
        size_t i1 = std::min(i0 + chunksize, nrows);
        try {
          ctx->run(i0, i1);
          // if (ith == 0) progress.report(i1);
        } catch (...) {
          oem.capture_exception();
          stop_iteration = true;
        }
        #pragma omp ordered
        {
          try {
            ctx->commit();
          } catch (...) {
            oem.capture_exception();
            stop_iteration = true;
          }
        }
      }
    }
    oem.rethrow_exception_if_any();
  }
}


}  // namespace dt
