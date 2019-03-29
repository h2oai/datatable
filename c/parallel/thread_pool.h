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
#ifndef dt_PARALLEL_THREAD_POOL_h
#define dt_PARALLEL_THREAD_POOL_h
#include <mutex>               // std::mutex
#include <vector>              // std::vector
#include "parallel/thread_scheduler.h"
#include "parallel/thread_worker.h"
namespace dt {
using std::size_t;



/**
 * Pool of threads, capable of executing a given work load in parallel.
 *
 * The pool contains a certain number of `thread_worker`s, each running on its
 * own thread. The number of workers can be adjusted up or down using method
 * `set_number_of_threads()`.
 *
 * Normally, the thread pool is in the "sleeping" state. This means all workers
 * are idling, consuming `thread_sleep_task`s from the `thread_sleep_scheduler`.
 *
 * However, once a user requests `execute_job()`, the threads are awaken and
 * use the supplied scheduler to perform the job. The method `execute_job()` is
 * blocking, and will return only after the job is completely finished and the
 * thread pool has been put back to sleep.
 *
 * The main thread of execution (which runs `execute_job()` and is connected to
 * Python) does not perform any tasks. Instead, it only performs high-level
 * maintenance: progress reporting, message logs, exception handling, checking
 * for user interrupts, etc.
 *
 */
class thread_pool {
  private:
    // Worker instances, each running on its own thread. Each thread has a
    // reference to its own worker, so these workers must be alive as long
    // as their corresponding threads are running.
    // Also, use pointers here instead of `thread_worker` objects, so that
    // the pointers to each worker remain constant when the vector resizes
    // (these pointers are stored within each thread).
    std::vector<std::unique_ptr<thread_worker>> workers;

    // Schedulers used for thread pool management.
    thread_sleep_scheduler    sch_sleep;
    thread_shutdown_scheduler sch_shutdown;

  public:
    thread_pool();
    thread_pool(const thread_pool&) = delete;
    thread_pool(thread_pool&&) = delete;
    ~thread_pool();

    void execute_job(thread_scheduler&);

    size_t size() const noexcept;
    void resize(size_t n);
};



/**
 * thread_pool is a singleton, returned by this function
 */
thread_pool& get_thread_pool();


/**
 * Return the number of concurrent threads supported by the machine. This
 * value is approximate. If the number of concurrent threads is unknown,
 * this function returns 1.
 */
size_t get_hardware_concurrency() noexcept;


}  // namespace dt
#endif
