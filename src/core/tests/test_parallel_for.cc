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
#ifdef DTTEST
#include <atomic>
#include <vector>
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dttest {


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



void test_parallel_for_ordered(size_t n) {
  std::atomic_flag executing_ordered = ATOMIC_FLAG_INIT;
  std::vector<size_t> done(n, 0);
  std::atomic<int> ordered_section {0};
  std::atomic<size_t> next_ordered {0};

  dt::parallel_for_ordered(
    /* n_iters = */ n,
    /* n_threads = */ dt::NThreads(),
    [&] (dt::ordered* o) {
      std::atomic_flag executing_local = ATOMIC_FLAG_INIT;
      int k = ordered_section.fetch_add(1);

      o->parallel(
        [&](size_t j) {
          if (executing_local.test_and_set()) {
            throw AssertionError() << "Frame " << k
              << " is executed in another thread, start = " << j;
          }
          ASSERT_LT(j, n);
          ASSERT_EQ(done[j], 0);
          done[j] = 1;
          executing_local.clear();
        },
        [&](size_t j) {
          if (executing_ordered.test_and_set()) {
            throw AssertionError() << "Another thread is executing ordered section";
          }
          if (executing_local.test_and_set()) {
            throw AssertionError() << "Frame " << k << " is executed in another thread, ordered = " << j;
          }
          // body of ordered section
          ASSERT_EQ(next_ordered, j);
          next_ordered++;
          ASSERT_EQ(done[j], 1);
          done[j] = 2;
          // end of ordered section
          executing_local.clear();
          executing_ordered.clear();
        },
        [&](size_t j) {
          if (executing_local.test_and_set()) {
            throw AssertionError() << "Frame " << k << " is executed in another thread, final = " << j;
          }
          ASSERT_EQ(done[j], 2);
          done[j] = 3;
          executing_local.clear();
        });

      if (executing_local.test_and_set()) {
        throw AssertionError() << "Exiting parallel, while another thread is "
            "still executing this frame";
      }
    });

  ASSERT_EQ(next_ordered, n);
  for (size_t i = 0; i < n; ++i) {
    ASSERT_EQ(done[i], 3);
  }
}



}  // namespace dttest
#endif
