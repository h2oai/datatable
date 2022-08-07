//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
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
#include <atomic>
#include <vector>
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dt {
namespace tests {


TEST(parallel, for_static) {
  constexpr size_t n = 1000;
  for (size_t nth = 0; nth <= dt::num_threads_in_pool() * 2; ++nth) {
    std::vector<size_t> data(n, 0);

    dt::parallel_for_static(n, dt::ChunkSize(1), dt::NThreads(nth),
      [&](size_t i) {
        data[i] += 1 + 2 * i;
      });

    for (size_t i = 0; i < n; ++i) {
      ASSERT_EQ(data[i], 1 + 2*i);
    }
  }
}


TEST(parallel, for_dynamic) {
  constexpr size_t n = 1000;
  for (size_t nth = 0; nth <= dt::num_threads_in_pool(); ++nth) {
    std::vector<size_t> data(n, 0);

    dt::parallel_for_dynamic(n, dt::NThreads(nth),
      [&](size_t i) {
        data[i] += 1 + 2 * i;
      });

    for (size_t i = 0; i < n; ++i) {
      ASSERT_EQ(data[i], 1 + 2*i);
    }
  }
}



TEST(parallel, for_dynamic_nested) {
  constexpr size_t n = 1000;
  std::vector<size_t> data(n, 0);
  std::atomic<size_t> total { 0 };

  dt::parallel_region([&] {
    size_t counter = 0;

    dt::parallel_for_dynamic(n,
      [&](size_t i) {
        data[i] += 1 + 2 * i;
        counter++;
      });

    total += counter;
  });

  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(data[i], 1 + 2*i);
  }
  ASSERT_EQ(total.load(), n);
}



}}  // namespace dt::tests
#endif
