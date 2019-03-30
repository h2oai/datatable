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
#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <thread>              // std::this_thread
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_worker.h"
#include "utils/assert.h"
namespace dt {



thread_scheduler::~thread_scheduler() {}


void thread_scheduler::abort_execution() {
  // default implementation does nothing
}




//------------------------------------------------------------------------------
// once scheduler
//------------------------------------------------------------------------------

once_scheduler::once_scheduler(size_t nth, thread_task* task_)
  : done(nth, 0),
    task(task_) {}


thread_task* once_scheduler::get_next_task(size_t i) {
  if (done[i].v) {
    return nullptr;
  }
  done[i].v = 1;
  return task;
}




void run_once_per_thread(function<void(size_t)> f) {
  thread_pool& thpool = get_thread_pool();
  simple_task task(f);
  once_scheduler sch(thpool.size(), &task);
  thpool.execute_job(&sch);
}



}  // namespace dt
