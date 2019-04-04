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
#include "utils/exceptions.h"
#include "ztest.h"
namespace dttest {


void test_parallel_for_dynamic(size_t n) {
  std::vector<size_t> data(n, 0);

  dt::parallel_for_dynamic(n,
    [&](size_t i) {
      data[i] += 1 + 2 * i;
    });

  for (size_t i = 0; i < n; ++i) {
    if (data[i] != 1 + 2*i) {
      throw AssertionError() << "Incorrect data[" << i << "] = " << data[i]
        << " in test_parallel_for_dynamic(), expected " << 1 + 2*i;
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



}  // namespace dttest
#endif
