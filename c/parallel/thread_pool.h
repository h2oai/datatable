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
#ifndef dt_PARALLEL_THREAD_POOL_h
#define dt_PARALLEL_THREAD_POOL_h
#include <mutex>               // std::mutex
#include <vector>              // std::vector
#include "parallel/thread_task_scheduler.h"
#include "parallel/thread_worker.h"
namespace dt {
using std::size_t;



/**
 * Pool of threads, capable of executing a given work load in parallel.
 *
 * The pool contains a certain number of `thread_worker`s, each running on its
 * own thread. The number of workers can be adjusted up or down using method
 * `set_number_of_threads(n)`.
 *
 * At any given time, the pool can be either "sleeping", or "active". While
 * sleeping, the pool uses `thread_sleep_scheduler`, which emits "sleep" tasks.
 * This is the initial state of the thread pool, and it returns to this state
 * after each call to `execute_job()`.
 *
 */
class thread_pool {
  private:
    // Worker instances, each running on its own thread.
    std::vector<thread_worker> workers;

    // Scheduler instances used for thread pool management.
    thread_sleep_scheduler    sch_sleep;
    thread_shutdown_scheduler sch_shutdown;

  public:
    thread_pool();
    thread_pool(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    ~thread_pool();

    void execute_job(thread_task_scheduler*);
    void set_number_of_threads(size_t n);
};


// thread_pool is a singleton, returned by this function
thread_pool& get_thread_pool();


}  // namespace dt
#endif
