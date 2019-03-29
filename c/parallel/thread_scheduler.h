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
#include <vector>      // std::vector
#include "parallel/thread_worker.h"
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
// thread sleep scheduler
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
 *   - unlock the mutex and notify all threads waiting on `tsleep[0].alarm`.
 *
 * As the threads awaken, they check their task's `next_scheduler` property, see
 * that it is now not-null, they will switch to that scheduler and finish their
 * current sleep task. Note that it may take some time for OS to notify and
 * awaken all the threads; some threads may already finish their new task by the
 * time the last thread in the team gets up.
 *
 * When a thread's queue is exhausted and there are no more tasks to do, that
 * worker receives a `nullptr` from `get_next_task()`. At this moment the
 * worker switches back to `thread_sleep_scheduler`, and requests a task. The
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
class thread_sleep_scheduler : public thread_scheduler {
  private:
    static constexpr size_t N_SLEEP_TASKS = 2;
    thread_sleep_task tsleep[N_SLEEP_TASKS];
    // Index within array `tsleep` indicating which sleep task is "current";
    // where "current" means that all sleeping threads are waiting on the
    // condition variable within that task, and its `n_sleeping_threads` holds
    // the total count of workers that are currently sleeping.
    size_t index = 0;

  public:
    thread_task* get_next_task(size_t thread_index) override;

    void awaken(thread_scheduler* next);

    // Called from the master thread, this function will block until all the
    // work is finished and all worker threads have been put to sleep. If there
    // was an exception during execution of any of the tasks, this exception
    // will be rethrown here (but only after all workers were put to sleep).
    void join(size_t nthreads);

  private:
    friend class thread_shutdown_scheduler;
    void pretend_thread_went_to_sleep();
};




//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

class thread_shutdown_scheduler : public thread_scheduler {
  private:
    size_t n_threads_to_keep;
    thread_sleep_scheduler* sleep_scheduler;
    shutdown_thread_task shutdown;

  public:
    thread_shutdown_scheduler(size_t nnew, thread_sleep_scheduler*);
    thread_task* get_next_task(size_t thread_index) override;
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
