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
#include "parallel/job_idle.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "parallel/api.h"
#include "parallel/thread_pool.h"
namespace dt {




//------------------------------------------------------------------------------
// "worker controller" scheduler
//------------------------------------------------------------------------------

Job_Idle::Job_Idle() {
  curr_sleep_task = new SleepTask(this);
  prev_sleep_task = new SleepTask(this);
  n_threads_running = 0;
}


ThreadTask* Job_Idle::get_next_task(size_t) {
  return curr_sleep_task;
}


/**
 * When this method is run, all other threads are inside the
 * `curr_sleep_task->execute()` method. When we modify variables
 * `curr_sleep_task` / `prev_sleep_task`, we do so under the
 * protection of a mutex, ensuring that the other threads cannot
 * attempt to read those variables at this time. The other methods
 * of this class can treat these variables as if they were constant,
 * meaning that they can be safely read even without mutex protection.
 *
 * The second part of this method (after the lock is unlocked) is
 * already multi-threaded: at that point other threads wake up and
 * may call arbitrary API of `Job_Idle`.
 *
 * Note that we set the variable `n_threads_running` explicitly here
 * (as opposed to, say, allowing each thread to increment this counter
 * upon awaking). This is necessary, because we want to prevent the
 * situation where the OS would delay waking up the threads, so that
 * by the time we run `join()` the number of running threads would be
 * zero, even though no work has been done yet.
 */
void Job_Idle::awaken_and_run(ThreadJob* job, size_t nthreads) {
  xassert(job);
  xassert(this_thread_index() == 0);
  xassert(n_threads_running == 0);
  xassert(prev_sleep_task->next_scheduler == nullptr);
  xassert(curr_sleep_task->next_scheduler == nullptr);
  int nth = static_cast<int>(nthreads) - 1;

  std::swap(curr_sleep_task, prev_sleep_task);
  n_threads_running += nth;
  saved_exception = nullptr;

  prev_sleep_task->next_scheduler = job;
  prev_sleep_task->semaphore.signal(nth);
  // enable_monitor(true);
  master_worker->run_master(job);
}


// Wait until all threads go back to sleep (which would mean the job is done)
void Job_Idle::join() {
  xassert(this_thread_index() == 0);
  // Busy-wait until all threads finish running
  while (n_threads_running.load() != 0);

  // Clear `.next_scheduler` flag of the previous sleep task, indicating that
  // we no longer run in a parallel region (see `is_running()`).
  prev_sleep_task->next_scheduler = nullptr;
  // enable_monitor(false);

  if (saved_exception) {
    progress::manager->reset_interrupt_status();
    std::rethrow_exception(saved_exception);
  }

  progress::manager->handle_interrupt();
}


void Job_Idle::set_master_worker(ThreadWorker* worker) noexcept {
  master_worker = worker;
}


void Job_Idle::on_before_thread_removed() {
  xassert(n_threads_running > 0);
  n_threads_running--;
}

void Job_Idle::on_before_thread_added() {
  n_threads_running++;
}


// Multiple threads may throw exceptions simultaneously, thus
// need to protect access to `saved_exception` with a mutex.
// In addition, `job->abort_execution()` is also protected,
// just in case, ensuring that only one thread can call that
// method at a time.
//
void Job_Idle::catch_exception() noexcept {
  try {
    std::lock_guard<std::mutex> lock(thpool->global_mutex());
    if (!saved_exception) {
      saved_exception = std::current_exception();
    }
    ThreadJob* current_job = prev_sleep_task->next_scheduler;
    if (current_job) {
      current_job->abort_execution();
    }
  } catch (...) {}
}


bool Job_Idle::is_running() const noexcept {
  return (prev_sleep_task->next_scheduler != nullptr);
}




//------------------------------------------------------------------------------
// SleepTask
//------------------------------------------------------------------------------

SleepTask::SleepTask(Job_Idle* ij)
  : controller(ij), next_scheduler{nullptr} {}

void SleepTask::execute() {
  controller->on_before_thread_removed();
  semaphore.wait();
  xassert(next_scheduler);
  thpool->assign_job_to_current_thread(next_scheduler);
}




}  // namespace dt
