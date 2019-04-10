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


static void test_barrier_1() {
  size_t nthreads = dt::num_threads_in_pool();
  std::vector<size_t> data_old(nthreads, 0);
  std::vector<size_t> data_new(nthreads, 0);

  dt::parallel_region(
    [&]() {
      size_t ithread = dt::this_thread_index();
      data_old[ithread] = ithread + 1;
      dt::barrier();
      size_t min = *std::min_element(std::begin(data_old), std::end(data_old));
      size_t max = *std::max_element(std::begin(data_old), std::end(data_old));
      data_new[ithread] = data_old[ithread] + min*max;
    });

  for (size_t i = 0; i < nthreads; ++i) {
    size_t reference_i = i + 1 + nthreads;
    if (data_new[i] != reference_i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data_new[i]
        << " in test_barrier_1(), expected " << reference_i;
    }
  }
}


static void test_barrier_2() {
  size_t nthreads = dt::num_threads_in_pool();
  std::vector<size_t> data(nthreads, 0);

  dt::parallel_region(
    [&]() {
      size_t ithread = dt::this_thread_index();
      data[ithread] = ithread + 1;
      dt::barrier();
      size_t max = *std::max_element(std::begin(data), std::end(data));
      size_t min = *std::min_element(std::begin(data), std::end(data));
      dt::barrier();
      data[ithread] += min*max;
    });

  for (size_t i = 0; i < nthreads; ++i) {
    size_t reference_i = i + 1 + nthreads;
    if (data[i] != reference_i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
        << " in test_barrier_2(), expected " << reference_i;
    }
  }
}


static void test_barrier_n(size_t n) {
  size_t nthreads = dt::num_threads_in_pool();
  std::vector<size_t> data(nthreads, 0);

  dt::parallel_region(
    [&]() {
      size_t ithread = dt::this_thread_index();
      size_t min = 0;
      size_t max = 0;
      for (size_t i = 0; i < n; ++i) {
        data[ithread] += 1 + (min != i) + (max != i);
        dt::barrier();
        min = *std::min_element(std::begin(data), std::end(data));
        max = *std::max_element(std::begin(data), std::end(data));
        dt::barrier();
      }
    });

  for (size_t i = 0; i < nthreads; ++i) {
    size_t reference_i = n;
    if (data[i] != reference_i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
        << " in test_barrier_n(), expected " << reference_i;
    }
  }
}


void test_barrier(size_t n) {
  test_barrier_1();
  test_barrier_2();
  test_barrier_n(n);
}


}  // namespace dttest
#endif
