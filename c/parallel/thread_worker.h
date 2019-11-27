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
#ifndef dt_PARALLEL_THREAD_WORKER_h
#define dt_PARALLEL_THREAD_WORKER_h
#include <atomic>               // std::atomic
#include <condition_variable>   // std::condition_variable
#include <cstddef>              // std::size_t
#include <memory>               // std::unique_ptr
#include <mutex>                // std::mutex
#include <thread>               // std::thread
#include "parallel/monitor_thread.h"
#include "parallel/semaphore.h"
#include "parallel/thread_scheduler.h"
namespace dt {
using std::size_t;

// Forward-declare
class idle_job;


/**
 * A class that encapsulates thread-specific runtime information. After
 * instantiation, we expect this class to be accessed within its own thread
 * only. This makes it safe to have variables such as `scheduler` non-atomic.
 *
 * Any communication with the worker (including changing to a new scheduler)
 * is performed only via the current scheduler: the scheduler may emit a task
 * that changes the worker's state.
 *
 * The thread stops running when `scheduler` becomes nullptr.
 */
class thread_worker {
  friend class thread_shutdown_scheduler;
  friend class idle_job;

  private:
    const size_t thread_index;
    std::thread  thread;
    thread_scheduler*  scheduler;
    idle_job* controller;

  public:
    thread_worker(size_t i, idle_job*);
    thread_worker(const thread_worker&) = delete;
    thread_worker(thread_worker&&) = delete;
    ~thread_worker();

    void run() noexcept;
    void run_master(thread_scheduler*) noexcept;
    size_t get_index() const noexcept;
};




//------------------------------------------------------------------------------
// worker controller (scheduler)
//------------------------------------------------------------------------------

/**
 * This class handles putting to sleep/awaking of workers in a thread pool.
 * A single instance of this class exists in `thread_pool`.
 *
 * Initially all workers in a thread pool are in the "idle" state, running the
 * sleep task returned by this scheduler. This sleep task is `tsleep[0]`, and
 * it contains a mutex, and a condition variable. In this state the workers are
 * simply waiting, though they may occasionally be woken by the operating system
 * to check whether `tsleep[0].next_scheduler` became non-null.
 *
 * More precisely, a thread is considered to be asleep if its scheduler is this
 * class, and if the thread already requested a sleep task from this scheduler
 * and started executing that sleep task.
 *
 * When master thread calls `awaken` (and only the master thread is allowed to
 * do so), we do the following:
 *   - lock `tsleep[0].mutex` (at this point no thread can awaken, even
 *     spuriously, because they would need to acquire lock on the same mutex as
 *     they wake up);
 *   - set `tsleep[0].next_scheduler` to the job that needs to be executed;
 *   - set `tsleep[1].next_scheduler` to nullptr;
 *   - change `index` from 0 to 1;
 *   - unlock the mutex and notify all threads waiting on
 *     `tsleep[0].wakeup_all_threads_cv`.
 *
 * As the threads awaken, they check their task's `next_scheduler` property, see
 * that it is now not-null, they will switch to that scheduler and finish their
 * current sleep task. Note that it may take some time for OS to notify and
 * awaken all the threads; some threads may already finish their new task by the
 * time the last thread in the team gets up.
 *
 * When a thread's queue is exhausted and there are no more tasks to do, that
 * worker receives a `nullptr` from `get_next_task()`. At this moment the
 * worker switches back to `idle_job`, and requests a task. The
 * thread sleep scheduler will now return `tsleep[1]`, which has its own mutex
 * and a condition variable, and its `.next_scheduler` is null, indicating the
 * sleeping state. This will allow the thread to go safely to sleep, while other
 * threads might still be waking up from the initial sleep.
 *
 * The master thread that called `awaken(job)` will then call `job.join()`,
 * and it is the responsibility of thread_scheduler `job` to wait until all
 * threads have finished execution and were put back to sleep. Thus, the master
 * thread ensures that all threads are sleeping again before the next call to
 * `awaken`.
 */
class idle_job : public thread_scheduler {
  friend std::mutex& python_mutex();
  private:
    struct sleep_task : public thread_task {
      idle_job* const controller;
      thread_scheduler* next_scheduler;
      LightweightSemaphore semaphore;

      sleep_task(idle_job*);
      void execute(thread_worker* worker) override;
    };

    // "Current" sleep task, meaning that all sleeping threads are executing
    // `curr_sleep_task->execute()`.
    sleep_task* curr_sleep_task;

    // The "previous" sleep task. The pointers `prev_sleep_task` and
    // `curr_sleep_task` flip-flop.
    sleep_task* prev_sleep_task;

    // Global mutex
    std::mutex mutex;

    // How many threads are currently active (i.e. not sleeping)
    std::atomic<int> n_threads_running;
    int : 32;

    // If an exception occurs during execution, it will be saved here
    std::exception_ptr saved_exception;

    // Thread-worker object corresponding to the master thread.
    thread_worker* master_worker;

  public:
    idle_job();

    thread_task* get_next_task(size_t thread_index) override;

    // Called from the master thread, this function will awaken all threads
    // in the thread pool, and give them `job` to execute.
    // Precondition: that all threads in the pool are currently sleeping.
    void awaken_and_run(thread_scheduler* job, size_t nthreads);

    // Called from the master thread, this function will block until all the
    // work is finished and all worker threads have been put to sleep. If there
    // was an exception during execution of any of the tasks, this exception
    // will be rethrown here (but only after all workers were put to sleep).
    void join();

    // Called from worker threads, within the `catch(...){ }` block, this method
    // is used to signal that an exception have occurred. The method will save
    // this exception, so that it can be re-thrown after the parallel region
    // exits.
    void catch_exception() noexcept;

    // Return true if there is a task currently being run in parallel.
    bool is_running() const noexcept;

    // This function should be called before a new thread is spawned.
    void on_before_thread_added();

    void set_master_worker(thread_worker*) noexcept;

    // This callback should be called before a thread is removed from the
    // threadpool.
    void on_before_thread_removed();
};




//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

class thread_shutdown_scheduler : public thread_scheduler {
  private:
    struct shutdown_task : public thread_task {
      void execute(thread_worker* worker) override;
    };

    size_t n_threads_to_keep;
    idle_job* controller;
    shutdown_task shutdown;

  public:
    thread_shutdown_scheduler(size_t nnew, idle_job*);
    thread_task* get_next_task(size_t thread_index) override;
};


// Helper function, defined in `thread_pool.cc`.
// This should only be called within a new thread.
void _set_thread_num(size_t);

}  // namespace dt
#endif
