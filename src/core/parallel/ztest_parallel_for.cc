//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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



void test_parallel_for_ordered(size_t n)
{
  std::atomic_flag global = ATOMIC_FLAG_INIT;
  std::vector<size_t> done(n, 0);
  std::atomic<size_t> next_ordered {0};

  class OTask : public dt::OrderedTask2 {
    private:
      std::atomic_flag& executing_global_;
      std::vector<size_t>& done_;
      std::atomic<size_t>& next_ordered_;
      std::atomic_flag executing_local_ = ATOMIC_FLAG_INIT;
      size_t : 56;

    public:
      OTask(std::atomic_flag& global,
            std::atomic<size_t>& next_ordered, sztvec& done)
        : executing_global_(global),
          done_(done),
          next_ordered_(next_ordered) {}

      void start(size_t j) override {
        XAssert(!executing_local_.test_and_set());
        XAssert(j < done_.size());
        XAssert(!done_[j]);
        done_[j] = 1;
        executing_local_.clear();
      }

      void order(size_t j) override {
        XAssert(!executing_global_.test_and_set());
        XAssert(!executing_local_.test_and_set());
        XAssert(next_ordered_ == j);
        next_ordered_++;
        XAssert(done_[j] == 1);
        done_[j] = 2;
        executing_local_.clear();
        executing_global_.clear();
      }

      void finish(size_t j) override {
        XAssert(!executing_local_.test_and_set());
        XAssert(done_[j] == 2);
        done_[j] = 3;
        executing_local_.clear();
      }
  };

  dt::parallel_for_ordered2(n, dt::NThreads(),
    [&]{
      return std::make_unique<OTask>(global, next_ordered, done);
    });

  XAssert(next_ordered == n);
  for (size_t i = 0; i < n; ++i) {
    XAssert(done[i] == 3);
  }
}



}  // namespace dttest
#endif
