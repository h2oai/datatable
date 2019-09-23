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
#include <atomic>
#include <vector>
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "ztest.h"
namespace dttest {


void test_parallel_for_static(size_t n) {
  for (size_t nth = 0; nth <= dt::num_threads_in_pool() * 2; ++nth) {
    std::vector<size_t> data(n, 0);

    dt::parallel_for_static(n, dt::ChunkSize(1), dt::NThreads(nth),
      [&](size_t i) {
        data[i] += 1 + 2 * i;
      });

    for (size_t i = 0; i < n; ++i) {
      if (data[i] != 1 + 2*i) {
        throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
          << " in test_parallel_for_static() for nth = " << nth
          << ", expected " << 1 + 2*i;
      }
    }
  }
}


void test_parallel_for_dynamic(size_t n) {
  for (size_t nth = 0; nth <= dt::num_threads_in_pool(); ++nth) {
    std::vector<size_t> data(n, 0);

    dt::parallel_for_dynamic(n, dt::NThreads(nth),
      [&](size_t i) {
        data[i] += 1 + 2 * i;
      });

    for (size_t i = 0; i < n; ++i) {
      if (data[i] != 1 + 2*i) {
        throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
          << " in test_parallel_for_dynamic() for nth = " << nth
          << ", expected " << 1 + 2*i;
      }
    }
  }
}



void test_parallel_for_dynamic_nested(size_t n) {
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
    if (data[i] != 1 + 2*i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
        << " in test_parallel_for_dynamic_nested(), expected " << 1 + 2*i;
    }
  }
  size_t nn = total.load();
  if (nn != n) {
    throw AssertionError() << "Incorrect total = " << nn
        << " in test_parallel_for_dynamic_nested(), expected " << n;
  }
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
          if (j >= n) throw AssertionError() << "Invalid iteration index " << j;
          if (done[j]) {
            throw AssertionError() << "Iteration " << j
              << " was already executed before: done=" << done[j];
          }
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
          if (next_ordered != j) {
            throw AssertionError() << "Wrong ordered iteration " << j << ", expected " << next_ordered;
          }
          next_ordered++;
          if (done[j] != 1) {
            throw AssertionError() << "Iteration " << j << " was ordered with done=" << done[j];
          }
          done[j] = 2;
          // end of ordered section
          executing_local.clear();
          executing_ordered.clear();
        },
        [&](size_t j) {
          if (executing_local.test_and_set()) {
            throw AssertionError() << "Frame " << k << " is executed in another thread, final = " << j;
          }
          if (done[j] != 2) {
            throw AssertionError() << "Iteration " << j << " is finalized with done=" << done[j];
          }
          done[j] = 3;

          executing_local.clear();
        });

      if (executing_local.test_and_set()) {
        throw AssertionError() << "Exiting parallel, while another thread is "
            "still executing this frame";
      }
    });

  if (next_ordered != n) {
    throw AssertionError() << "Only " << next_ordered
        << " iterations were ordered, out of " << n;
  }
  for (size_t i = 0; i < n; ++i) {
    if (done[i] != 3) {
      throw AssertionError() << "Iteration " << i
        << " was not run correctly: " << done[i];
    }
  }
}



}  // namespace dttest
#endif
