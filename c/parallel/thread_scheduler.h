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
#include <atomic>      // std::atomic
#include <memory>      // std::unique_ptr
#include <mutex>       // std::mutex
#include <vector>      // std::vector
#include "parallel/thread_task.h"
#include "utils/function.h"
#include "utils/macros.h"
namespace dt {
using std::size_t;



//------------------------------------------------------------------------------
// Base scheduler
//------------------------------------------------------------------------------

class thread_scheduler {
  protected:
    std::mutex mtx;
    std::exception_ptr saved_exception;

  public:
    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    // Called from worker threads, within the `catch(...){ }` block, this method
    // is used to signal that an exception have occurred. The method will save
    // this exception and then call `abort_execution()` (see below). The saved
    // exception will be re-thrown at the end of `join()`.
    void handle_exception() noexcept;


    //--------------------------------------------------------------------------
    // Subclass API
    //--------------------------------------------------------------------------

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
};




//------------------------------------------------------------------------------
// once scheduler
//------------------------------------------------------------------------------

class once_scheduler : public thread_scheduler {
  private:
    std::vector<cache_aligned<size_t>> done;
    thread_task* task;

  public:
    once_scheduler(size_t nthreads, thread_task*);
    thread_task* get_next_task(size_t thread_index) override;
};


// Call function `f` exactly once in each thread.
void run_once_per_thread(function<void(size_t)> f);



}  // namespace dt
#endif
