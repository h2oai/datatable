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
#include <iostream>
namespace dttest {


void test_progress(size_t n, size_t nth) {
  std::vector<size_t> data(n, 0);
  dt::progress::work job(n / nth);
  job.set_message("Starting...");

  dt::parallel_for_static(n, dt::ChunkSize(n / nth), dt::NThreads(nth),
    [&](size_t i) {
      data[i] += 1 + 2 * i;
      if (dt::this_thread_index() == 0) {
        job.set_message("Running...");
        job.set_done_amount(i);
      }
    });

  job.set_message("Finishing...");
}


}  // namespace dttest
#endif
