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
#include "parallel/thread_task_scheduler.h"
#include "parallel/thread_worker.h"
namespace dt {


task::~task() {}

thread_task_scheduler::~thread_task_scheduler() {}



//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

thread_shutdown_scheduler::shutdown_thread_task::shutdown_thread_task(size_t i)
  : thread_index(i) {}

void thread_shutdown_scheduler::shutdown_thread_task::execute() {
  thread_pool& thpool = get_thread_pool();
  thread_worker* pworker = &thpool.get_worker(thread_index);
  pworker->shutdown();
}


thread_shutdown_scheduler::thread_shutdown_scheduler(size_t i0)
  : n_threads_to_keep(i0) {}


task* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep) {
    // return gotosleep task
  } else {
    shutdown_tasks.emplace_back(thread_index);
    return &shutdown_tasks.back();
  }
}


}  // namespace dt
