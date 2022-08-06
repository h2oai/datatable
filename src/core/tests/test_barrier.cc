//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifdef DT_TEST
#include <algorithm>
#include <vector>
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dt {
namespace tests {


TEST(parallel, barrier1) {
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
    ASSERT_EQ(data_new[i], reference_i);
  }
}


TEST(parallel, barrier2) {
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
    ASSERT_EQ(data[i], reference_i);
  }
}


TEST(parallel, barrierN) {
  size_t n = 100;
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
    ASSERT_EQ(data[i], reference_i);
  }
}



}}  // namespace dt::tests
#endif
