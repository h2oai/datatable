//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_PARALLEL_THREAD_POOL_h
#define dt_PARALLEL_THREAD_POOL_h
#include <mutex>               // std::mutex
#include <thread>              // std::thread::id
#include <vector>              // std::vector
#include "parallel/job_idle.h"
#include "parallel/thread_job.h"
#include "parallel/thread_worker.h"
namespace dt {

class ThreadTeam;
class PythonLock;



/**
  * Pool of threads, capable of executing a given workload in
  * parallel.
  *
  * The pool contains a certain number of `ThreadWorker`s, each
  * running on its own thread, except for the 0th worker which
  * represents the main thread and should be invoked manually. The
  * number of workers can be adjusted up or down using `resize()`.
  *
  * Normally, the thread pool is in the "sleeping" state. This means
  * all workers are idling, executing sleep tasks from the `Job_Idle`
  * (see documentation of this class in "ThreadWorker.h").
  *
  * However, once a user requests `execute_job()`, the threads are
  * awakened and use the supplied scheduler to perform the job. The
  * method `execute_job()` is blocking, and will return only after
  * the job is completely finished and the thread pool has been put
  * back to sleep.
  */
class ThreadPool
{
  friend class ThreadTeam;
  friend class Job_Idle;
  friend class PythonLock;

  private:
    // Worker instances, each running on its own thread. Each thread has a
    // reference to its own worker, so these workers must be alive as long
    // as their corresponding threads are running.
    // Also, use pointers here instead of `ThreadWorker` objects, so that
    // the pointers to each worker remain constant when the vector resizes
    // (these pointers are stored within each thread).
    std::vector<ThreadWorker*> workers_;

    // Number of threads in the pool, as requested by the user. This is
    // usually the same as `workers.size()` except in two cases: (1) when
    // the ThreadPool was just created: we don't spawn the workers until
    // executing the first parallel task, so before then the workers array
    // is empty; (2) while executing shutdown job, this variable reflects
    // the "new" number of threads that should be, whereas `workers` array
    // still holds as many workers as we had originally.
    size_t num_threads_requested_;

    // Scheduler used to manage sleep/awake cycle of the workers in the pool.
    // See definition in job_idle.h
    Job_Idle idle_job_;

    // Mutex which can be used to guard operations that must be protected
    // across all threads.
    std::mutex global_mutex_;

    // TODO: merge ThreadTeam functionality into the pool?
    ThreadTeam* current_team;

  public:
    ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;  // Not moveable: workers hold pointers to this->idle_job_.
    ~ThreadPool() = delete;             // ThreadPool never gets deleted

    static ThreadTeam* get_team_unchecked() noexcept;


    void execute_job(ThreadJob*);

    size_t size() const noexcept;
    void resize(size_t n);

    bool in_parallel_region() const noexcept;
    size_t n_threads_in_team() const noexcept;

    #ifndef DT_DISABLE
      static void init_options();
    #endif

    std::mutex& global_mutex();

    // This should only be called by `sleep_task` and `shutdown_task`.
    void assign_job_to_current_thread(ThreadJob*);

    bool get_use_semaphore();
    void set_use_semaphore(bool);

  private:
    void instantiate_threads();
};


extern ThreadPool* thpool;

// [Internal] This should only be called when starting a new thread.
void _set_thread_num(size_t);



}  // namespace dt
#endif
