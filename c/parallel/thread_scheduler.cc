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
#include "parallel/api.h"               // this_thread_index
#include "parallel/thread_scheduler.h"  // thread_scheduler
#include "parallel/thread_task.h"       // thread_task
namespace dt {



thread_scheduler::~thread_scheduler() {}


void thread_scheduler::abort_execution() {
  // default implementation does nothing
}


void thread_scheduler::execute_in_current_thread() {
  // If this throws an exception, it will propagate to outer level, where the
  // exception will get caught in the outer's level task executor.
  size_t ith = dt::this_thread_index();
  while (true) {
    auto task = get_next_task(ith);
    if (!task) break;
    task->execute(nullptr);
  }
}



}  // namespace dt
