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
#ifndef dt_PARALLEL_THREAD_SCHEDULER_h
#define dt_PARALLEL_THREAD_SCHEDULER_h
#include <cstddef>  // size_t
namespace dt {
using std::size_t;

// forward-declare
class thread_worker;


class thread_task {
  public:
    thread_task() = default;
    virtual ~thread_task();
    virtual void execute(thread_worker*) = 0;
};



class thread_scheduler {
  public:
    virtual ~thread_scheduler();

    // Invoked by a worker (on a worker thread), this method should return the
    // next task to be executed by thread `thread_index`. The returned pointer's
    // lifetime should be at least until the next invocation of
    // `get_next_task()` by the thread with the same index.
    virtual thread_task* get_next_task(size_t thread_index) = 0;

    // Invoked by `handle_exception()` (and therefore on a worker thread), this
    // method should cancel all pending tasks, or as many as feasible, since
    // their results will not be needed. This call is not supposed to be
    // blocking. The default implementation does nothing (all scheduled tasks
    // continue being executed), which is allowed but sub-optimal.
    virtual void abort_execution();

    void execute_in_current_thread();
};



}  // namespace dt
#endif
