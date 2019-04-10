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
#include <algorithm>
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "ztest.h"
namespace dttest {


void test_barrier(size_t n) {
  size_t nthreads = dt::num_threads_in_pool();
  std::vector<size_t> data(nthreads, n);

  dt::parallel_region(
    [&]() {
      size_t ithread = dt::this_thread_index();
      data[ithread] += ithread;
      dt::barrier();
      std::vector<size_t>::iterator max = max_element(std::begin(data), std::end(data));
      data[ithread] += *max;
      dt::barrier();
      max = max_element(std::begin(data), std::end(data));
      data[ithread] += *max;
    });

  for (size_t i = 0; i < nthreads; ++i) {
    size_t data_i = n + i + 3*(n + nthreads - 1);
    if (data[i] != data_i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
        << " in test_barrier(), expected " << data_i;
    }
  }
}


}  // namespace dttest
#endif
