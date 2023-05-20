//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifndef dt_PARALLEL_JOB_IDLE_h
#define dt_PARALLEL_JOB_IDLE_h
#include <atomic>                 // std::atomic
#include <cstddef>                // std::size_t
#include <mutex>                  // std::mutex
#include "parallel/semaphore.h"   // LightweightSemaphore
#include "parallel/thread_job.h"  // ThreadJob, ThreadTask
#include <condition_variable>

namespace dt {

class SleepTask;


/**
  * This class handles putting to sleep/awaking of workers in the
  * thread pool. A single instance of this class exists in the class
  * `thread_pool`.
  *
  * Initially all workers in a thread pool are in the "idle" state,
  * running the sleep task returned by this job. This sleep task is
  * `curr_sleep_task`, and it contains a semaphore that all threads
  * are waiting for.
  *
  * More precisely, a thread is considered to be asleep if its
  * current job is Job_Idle, and if the thread already requested a
  * sleep task from this job and started executing that sleep task.
  *
  * When main thread calls `awaken` (and only the main thread is
  * allowed to do so), we do the following:
  *   - "current" and "previous" sleep tasks are swapped, so that
  *     when a thread awakens and then wants to go to sleep again,
  *     it would receive the new `current_sleep_task_`;
  *   - the `previous_sleep_task_` is signalled to awaken,
  *     incrementing the semaphore and thus allowing all sleeping
  *     tasks to stop waiting and perform the next instruction
  *     (which is to give a new `job` to the worker).
  *   - the main thread executes the `job` explicitly also.
  *
  * When the threads awaken from waiting on the semaphore, they will
  * acquire the new `job` and start executing it. Note that it may
  * take some time for OS to notify and awaken all the threads; some
  * may already finish their jobs by the time the last thread in the
  * team wakes up.
  *
  * When a job's queue is exhausted and there are no more tasks to do,
  * that job sends `nullptr` from `get_next_task()`. At this moment
  * the workers switch back to `Job_Idle`, and request a task. The
  * `Job_Idle` will return `current_sleep_task_`, which will wait on
  * its own semaphore. This mechanism allows the threads to go safely
  * to sleep, while other threads might still be waking up from their
  * previous sleep.
  *
  * The main thread that called `job.awaken()` will then call
  * `job.join()`, which causes Job_Idle to wait until all workers
  * have finished running their tasks and indicated that they are
  * going back to sleep. Thus, the main thread ensures that all
  * threads are sleeping again before the next call to `awaken`
  * may happen.
  */
class Job_Idle : public ThreadJob {
  private:
    // "Current" sleep task, meaning that all sleeping threads are
    // executing `curr_sleep_task->execute()`.
    SleepTask*         current_sleep_task_;

    // The "previous" sleep task. The pointers `prev_sleep_task` and
    // `curr_sleep_task` flip-flop.
    SleepTask*         previous_sleep_task_;

    // How many threads are currently active (i.e. not sleeping)
    std::atomic<int>   n_threads_running_;
    int : 32;

    // If an exception occurs during execution, it will be saved here
    std::exception_ptr saved_exception_;

  public:
    Job_Idle();
    ~Job_Idle() override;

    ThreadTask* get_next_task(size_t thread_index) override;

    // Called from the main thread, this function will awaken all
    // workers in the thread pool, and give them `job` to execute.
    // Precondition: that all thread workers in the pool are
    // currently sleeping.
    void awaken_and_run(ThreadJob* job, size_t nthreads);

    // Called from the main thread, this function will block until
    // all the work is finished and all worker threads went back to
    // sleep. If there was an exception during execution of any of
    // the tasks, this exception will be rethrown here (but only
    // after all workers are sleeping).
    void join();

    // Called from worker threads, within the `catch(...){ }` block,
    // this method is used to signal that an exception have occurred.
    // This exception will be stored, so that it can be re-thrown
    // later after the parallel region exits.
    void catch_exception() noexcept;

    // Return true if there is a job currently being run in parallel.
    bool is_running() const noexcept;

    // Register changes in the total number of currently active
    // threads.
    void add_running_thread();
    void remove_running_thread();
};


class SleepTask : public ThreadTask {
  private:
    Job_Idle* parent_;
    ThreadJob* job_;
    std::condition_variable cv_;
    std::mutex cv_m_;

  public:
    SleepTask(Job_Idle*);
    bool is_sleeping() const noexcept;

    void execute();
    void wake_up(ThreadJob* next_job);
    void fall_asleep();
    void abort_current_job();
};



}  // namespace dt
#endif
