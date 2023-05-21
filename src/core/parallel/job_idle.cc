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
#include "utils/assert.h"
#include "parallel/api.h"
#include "parallel/job_idle.h"
#include "parallel/thread_pool.h"
#include <condition_variable>
#include <mutex>
namespace dt {


//------------------------------------------------------------------------------
// "worker controller" scheduler
//------------------------------------------------------------------------------

Job_Idle::Job_Idle() {
  current_sleep_task_ = new SleepTask(this);
  previous_sleep_task_ = new SleepTask(this);
  n_threads_running_ = 0;
}


Job_Idle::~Job_Idle() {
  delete current_sleep_task_;
  delete previous_sleep_task_;
}


ThreadTask* Job_Idle::get_next_task(size_t) {
  return current_sleep_task_;
}


/**
 * When this method is run, all other threads are inside the
 * `current_sleep_task_->execute()` method. When we modify variables
 * `current_sleep_task_` / `previous_sleep_task_`, we do so under the
 * protection of a mutex, ensuring that the other threads cannot
 * attempt to read those variables at this time. The other methods
 * of this class can treat these variables as if they were constant,
 * meaning that they can be safely read even without mutex protection.
 *
 * The second part of this method (after the lock is unlocked) is
 * already multi-threaded: at that point other threads wake up and
 * may call arbitrary API of `Job_Idle`.
 *
 * Note that we set the variable `n_threads_running_` explicitly here
 * (as opposed to, say, allowing each thread to increment this counter
 * upon awaking). This is necessary, because we want to prevent the
 * situation where the OS would delay waking up the threads, so that
 * by the time we run `join()` the number of running threads would be
 * zero, even though no work has been done yet.
 */
void Job_Idle::awaken_and_run(ThreadJob* job, size_t nthreads) {
  xassert(job);
  xassert(this_thread_index() == 0);
  xassert(n_threads_running_ == 0);
  xassert(previous_sleep_task_->is_sleeping());
  xassert(current_sleep_task_->is_sleeping());
  int nth = static_cast<int>(nthreads) - 1;

  std::swap(current_sleep_task_, previous_sleep_task_);
  n_threads_running_ += nth;
  saved_exception_ = nullptr;

  previous_sleep_task_->wake_up(job);
  thpool->workers_[0]->run_in_main_thread(job);
}


// Wait until all threads go back to sleep (which would mean the job is done)
void Job_Idle::join() {
  xassert(this_thread_index() == 0);
  // Busy-wait until all threads finish running
  while (n_threads_running_.load() != 0);

  previous_sleep_task_->fall_asleep();

  if (saved_exception_) {
    #ifndef DT_DISABLE
      progress::manager->reset_interrupt_status();
    #endif
    std::rethrow_exception(saved_exception_);
  }

  #ifndef DT_DISABLE
    progress::manager->handle_interrupt();
  #endif
}


void Job_Idle::remove_running_thread() {
  xassert(n_threads_running_ > 0);
  n_threads_running_--;
}

void Job_Idle::add_running_thread() {
  n_threads_running_++;
}


// Multiple threads may throw exceptions simultaneously, thus
// need to protect access to `saved_exception_` with a mutex.
// In addition, `job->abort_execution()` is also protected,
// just in case, ensuring that only one thread can call that
// method at a time.
//
void Job_Idle::catch_exception() noexcept {
  try {
    std::lock_guard<std::mutex> lock(thpool->global_mutex());
    #ifndef DT_DISABLE
      progress::manager->set_interrupt();
    #endif
    if (!saved_exception_) {
      saved_exception_ = std::current_exception();
    }
    previous_sleep_task_->abort_current_job();
  }
  catch (...) {}
}


bool Job_Idle::is_running() const noexcept {
  return !previous_sleep_task_->is_sleeping();
}



//------------------------------------------------------------------------------
// SleepTask
//------------------------------------------------------------------------------

SleepTask::SleepTask(Job_Idle* idle_job)
  : parent_(idle_job),
    job_(nullptr) {}


void SleepTask::execute() {
  parent_->remove_running_thread();
  {
    std::unique_lock<std::mutex> lk(cv_m_);
    cv_.wait(lk,[&]{return job_ != nullptr;});
  }
  xassert(job_);
  thpool->assign_job_to_current_thread(job_);
}


void SleepTask::wake_up(ThreadJob* next_job) {
  {
    std::lock_guard<std::mutex> lk(cv_m_);
    job_ = next_job;
  }
  cv_.notify_all();
}


void SleepTask::fall_asleep() {
  std::lock_guard<std::mutex> lk(cv_m_);
  // Clear `job_` indicating that we no longer run in a parallel region.
  job_ = nullptr;
}


void SleepTask::abort_current_job() {
  std::lock_guard<std::mutex> lk(cv_m_);
  if (job_) {
    job_->abort_execution();
  }
}


bool SleepTask::is_sleeping() const noexcept {
  return (job_ == nullptr);
}



}  // namespace dt
