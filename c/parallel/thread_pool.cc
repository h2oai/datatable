//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include <thread>    // std::thread::hardware_concurrency
#include "parallel/thread_pool.h"
#include "parallel/thread_worker.h"
#include "utils/c+++.h"
namespace dt {



//------------------------------------------------------------------------------
// thread_pool
//------------------------------------------------------------------------------

thread_pool::thread_pool() {}

thread_pool::~thread_pool() {
  resize(0);
}


void thread_pool::resize(size_t n) {
  if (workers.size() == n) return;
  if (workers.size() < n) {
    workers.reserve(n);
    for (size_t i = workers.size(); i < n; ++i) {
      workers.push_back(
        make_unique<dt::thread_worker>(i, &controller));
    }
    // Wait until all threads are properly alive & safely asleep
    controller.join(n);
  }
  else {
    thread_shutdown_scheduler tss(n, &controller);
    execute_job(&tss);
    workers.resize(n);
  }
}


void thread_pool::execute_job(thread_scheduler* job) {
  controller.awaken_and_run(job);
  controller.join(workers.size());
}


size_t thread_pool::size() const noexcept {
  return workers.size();
}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

thread_pool& get_thread_pool() {
  static thread_pool tp;
  return tp;
}


size_t get_hardware_concurrency() noexcept {
  unsigned int nth = std::thread::hardware_concurrency();
  if (!nth) nth = 1;
  return static_cast<size_t>(nth);
}



}  // namespace dt
