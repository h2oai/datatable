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

class thread_team;


/**
 * Pool of threads, capable of executing a given workload in parallel.
 *
 * The pool contains a certain number of `thread_worker`s, each running on its
 * own thread, except for the 0th worker which represents the master thread and
 * should be invoked manually. The number of workers can be adjusted up or down
 * using `set_number_of_threads()`.
 *
 * Normally, the thread pool is in the "sleeping" state. This means all workers
 * are idling, consuming sleep tasks from the `idle_job` (see documentation of
 * this class in "thread_worker.h").
 *
 * However, once a user requests `execute_job()`, the threads are awakened and
 * use the supplied scheduler to perform the job. The method `execute_job()` is
 * blocking, and will return only after the job is completely finished and the
 * thread pool has been put back to sleep.
 *
 */
class thread_pool {
  friend class thread_team;
  friend std::mutex& python_mutex();
  public:
    std::unique_ptr<monitor_thread> monitor;

  private:
    // Worker instances, each running on its own thread. Each thread has a
    // reference to its own worker, so these workers must be alive as long
    // as their corresponding threads are running.
    // Also, use pointers here instead of `thread_worker` objects, so that
    // the pointers to each worker remain constant when the vector resizes
    // (these pointers are stored within each thread).
    std::vector<thread_worker*> workers;

    // Number of threads in the pool, as requested by the user. This is
    // usually the same as `workers.size()` except in two cases: (1) when
    // the thread_pool was just created: we don't spawn the workers until
    // executing the first parallel task, so before then the workers array
    // is empty; (2) while executing shutdown job, this variable reflects
    // the "new" number of threads that should be, whereas `workers` array
    // still holds as many workers as we had originally.
    size_t num_threads_requested;

    // Scheduler used to manage sleep/awake cycle of the workers in the pool.
    // See definition in thread_worker.h
    idle_job controller;

    // Mutex which can be used to guard operations that must be protected
    // across all threads.
    std::mutex global_mutex;

    // TODO: merge thread_team functionality into the pool?
    thread_team* current_team;

    // Create the monitor thread, if it was not created yet.
    void init_monitor_thread() noexcept;

  public:
    thread_pool();
    thread_pool(const thread_pool&) = delete;
    // Not moveable: workers hold pointers to this->controller.
    thread_pool(thread_pool&&) = delete;

    static thread_team* get_team_unchecked() noexcept;


    void instantiate_threads();
    void execute_job(thread_scheduler*);

    size_t size() const noexcept;
    void resize(size_t n);

    bool in_parallel_region() const noexcept;
    size_t n_threads_in_team() const noexcept;

    // Monitor thread control.
    void enable_monitor(bool) noexcept;
    bool is_monitor_enabled() noexcept;

    static void init_options();
};

extern thread_pool* thpool;


}  // namespace dt
#endif
