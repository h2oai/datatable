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
  dt::progress::work job(n/nth);
  job.set_message("Starting...");

  std::vector<size_t> data(n, 0);

  dt::parallel_for_static(n, dt::ChunkSize(10), dt::NThreads(nth),
    [&](size_t i) {
      for (size_t j = 1; j < 100500; ++j) {
        data[i] += i % j;
      }

      if (dt::this_thread_index() == 0) {
        job.set_message("Running parallel_for_static...");
        job.add_done_amount(1);
      }
    });

  job.set_message("Finishing...");
  job.done();
}


void test_progress_nested(size_t n, size_t nth) {
  size_t n_iters = 10;
  dt::progress::work job(n_iters * (n/nth));
  job.set_message("Starting...");

  std::vector<size_t> data(n, 0);

  dt::parallel_region(nth,
    [&]() {


      for (size_t a = 0; a < n_iters; ++a) {
        dt::nested_for_static(n,
          [&](size_t i) {
            for (size_t j = 1; j < 100500; ++j) {
              data[i] += i % j;
            }

            if (dt::this_thread_index() == 0) {
              job.set_message("Running test_progress_nested...");
              job.add_done_amount(1);
            }
          });
      }
    });

  job.set_message("Finishing...");
  job.done();
}


void test_progress_dynamic (size_t n, size_t nth) {
  dt::progress::work job(n);
  job.set_message("Starting...");

  std::vector<size_t> data(n, 0);

  dt::parallel_for_dynamic(n, dt::NThreads(nth),
    [&](size_t i) {
      for (size_t j = 1; j < 100500; ++j) {
        data[i] += i % j;
      }

      if (dt::this_thread_index() == 0) {
        job.set_message("Running test_progress_dynamic...");
        job.add_done_amount(1);
      }
    });

  job.set_done_amount(n);
  job.set_message("Finishing...");
  job.done();
}


}  // namespace dttest
#endif
