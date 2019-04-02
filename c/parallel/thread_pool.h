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
#include <thread>              // std::thread::id
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
 * are idling, consuming sleep tasks from the `worker_controller`
 * (see documentation of `worker_controller` in "thread_worker.h").
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
    std::vector<thread_worker*> workers;

    // Number of threads requested by the user; however, we will only
    // instantiate the threads when `execute_job()` is first called.
    size_t num_threads_requested;

    // Scheduler used to manage sleep/awake cycle of the threads in the pool.
    worker_controller controller;

    // Thread id of the main (python) thread, mostly used to verify that
    // certain constructs are called from the master thread only.
    std::thread::id master_thread_id;

    // Singleton instance of the thread_pool, returned by `get_instance()`.
    static thread_pool* _instance;

  public:
    static thread_pool* get_instance();
    thread_pool();
    thread_pool(const thread_pool&) = delete;
    // Not moveable: workers hold pointers to this->controller.
    thread_pool(thread_pool&&) = delete;
    // ~thread_pool();

    void execute_job(thread_scheduler*);

    size_t size() const noexcept;
    void resize(size_t n);

    bool in_master_thread() const;

  private:
    void resize_impl();
    void cleanup_after_fork();
};



}  // namespace dt
#endif
