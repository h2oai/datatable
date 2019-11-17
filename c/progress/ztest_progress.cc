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
#ifdef DTTEST
#include <vector>
#include "parallel/api.h"
#include "progress/work.h"      // dt::progress::work
#include "utils/exceptions.h"
#include "ztest.h"
namespace dttest {


void test_progress_static(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  dt::progress::work job(n);
  job.set_message("Starting test_progress_static...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations = n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  job.set_message("Running test_progress_static...");
  dt::parallel_for_static(niterations, dt::ChunkSize(1), nthreads,
    [&](size_t i) {
      for (size_t j = 0; j < iteration_size; ++j) {
        data[i] += i % (j + 1);
      }
      if (dt::this_thread_index() == 0) {
        job.add_done_amount(1);
      }
    });

  job.set_message("Finishing test_progress_static...");
  job.done();
}


void test_progress_nested(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  const size_t nloops = 2;
  dt::progress::work job(nloops * n);
  job.set_message("Starting test_progress_nested...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  dt::parallel_region(nthreads,
    [&]() {
      if (dt::this_thread_index() == 0) {
        job.set_message("Running test_progress_nested...");
      }
      for (size_t l = 0; l < nloops; ++l) {
        dt::nested_for_static(niterations, dt::ChunkSize(1),
          [&](size_t i) {
            for (size_t j = 0; j < iteration_size; ++j) {
              data[i] += i % (j + 1);
            }
            if (dt::this_thread_index() == 0) {
              job.add_done_amount(1);
            }
          });
      }
      dt::barrier();
    });

  job.set_message("Finishing test_progress_nested...");
  job.done();
}


void test_progress_dynamic(size_t n, size_t nth) {
  const size_t iteration_size = 1000;
  dt::progress::work job(n);
  job.set_message("Starting test_progress_dynamic...");

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  job.set_message("Running test_progress_dynamic...");
  dt::parallel_for_dynamic(niterations, nthreads,
    [&](size_t i) {
      for (size_t j = 0; j < iteration_size; ++j) {
        data[i] += i % (j + 1);
      }

      if (dt::this_thread_index() == 0) {
        job.set_done_amount(i / nthreads.get());
      }
    });

  job.set_done_amount(n);
  job.set_message("Finishing test_progress_dynamic...");
  job.done();
}


void test_progress_ordered(size_t n, size_t nth) {
  const size_t iteration_size = 1000;

  dt::NThreads nthreads = dt::NThreads(nth);
  size_t niterations= n * nthreads.get();
  std::vector<size_t> data(niterations, 0);

  dt::parallel_for_ordered(
    /* n_iters = */ niterations,
    /* n_threads = */ nthreads,
    [&] (dt::ordered* o) {
      o->parallel(
        [&](size_t i) {
          for (size_t j = 0; j < iteration_size; ++j) {
            data[i] += i % (j + 1) + 1;
          }
        },
        [&](size_t i) {
          for (size_t j = 0; j < iteration_size; ++j) {
            data[i] += i % (j + 1) + 2;
          }
        },
        [&](size_t i) {
          for (size_t j = 0; j < iteration_size; ++j) {
            data[i] += i % (j + 1) + 3;
          }
        });
    });
}


}  // namespace dttest
#endif
