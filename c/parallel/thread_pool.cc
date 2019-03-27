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
#include "parallel/thread_pool.h"
#include "parallel/thread_worker.h"
namespace dt {



//------------------------------------------------------------------------------
// thread_pool
//------------------------------------------------------------------------------

thread_pool::thread_pool() {}

thread_pool::~thread_pool() {
  set_number_of_threads(0);
}


void thread_pool::set_number_of_threads(size_t n) {
  if (workers.size() == n) return;
  if (workers.size() < n) {
    for (size_t i = workers.size(); i < n; ++i) {
      workers.emplace_back(i, &sch_sleep);
    }
  }
  else {
    sch_shutdown.init(n, workers.size(), &sch_sleep);
    execute_job(&sch_shutdown);
    workers.resize(n);
  }
}


void thread_pool::execute_job(thread_task_scheduler* job) {
  sch_sleep.awaken(job);
  job->join();
}



// task* thread_pool::get_next_task(size_t i) {
//   return scheduler->get_next_task(i);
// }

// task* thread_pool::get_sleep_task() const {
//   return sleep_scheduler.get_next_task(0);
// }

// thread_worker& thread_pool::get_worker(size_t i) const {
//   return workers[i];
// }




thread_pool& get_thread_pool() {
  static thread_pool tp;
  return tp;
}




}  // namespace dt
