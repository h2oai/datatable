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
#include "parallel/thread_worker.h"
#ifndef DT_DISABLE
  #include "progress/progress_manager.h"  // dt::progress::progress_manager
  #include "utils/exceptions.h"
#endif
#include "utils/assert.h"
#include "parallel/api.h"
#include "parallel/thread_pool.h"
namespace dt {



/**
  * The worker creates its own thread of execution. The thread will
  * be executing function `ThreadWorker::run()` continuously until it
  * exits its loop.
  */
ThreadWorker::ThreadWorker(size_t i, Job_Idle* wc)
  : thread_index_(i),
    job_(wc),
    idle_job_(wc)
{
  if (i == 0) {
    job_ = nullptr;
    _set_thread_num(0);
  } else {
    // Create actual execution thread only when `this` is fully initialized
    wc->add_running_thread();
    thread_ = std::thread(&ThreadWorker::run, this);
  }
}


ThreadWorker::~ThreadWorker() {
  if (thread_.joinable()) thread_.join();
}



/**
  * This is the main function that will be run within the thread. It
  * continuously picks up tasks from the job_ and executes them. This
  * function stops running (terminating the thread) once `job_`
  * becomes nullptr.
  *
  * If the task returned from the `job_` is nullptr, then the thread
  * worker switches to "sleep" job and waits there for a signal from
  * the semaphore.
  */
void ThreadWorker::run() noexcept {
  _set_thread_num(thread_index_);
  while (job_) {
    try {
      ThreadTask* task = job_->get_next_task(thread_index_);
      if (task) {
        task->execute();
      } else {
        job_ = idle_job_;
      }
    }
    catch (...) {
      idle_job_->catch_exception();
    }
  }
}


/**
  * Similar to run(), but designed to run from the main thread. The
  * differences are the following:
  *   - this method does NOT run continuously, instead it starts with
  *     a new job, and finishes when the job is done.
  *   - the `job_` field is not used, instead the job is passed
  *     as a parameter.
  */
void ThreadWorker::run_in_main_thread(ThreadJob* job) noexcept {
  if (!job) return;
  while (true) {
    try {
      ThreadTask* task = job->get_next_task(0);
      if (!task) break;
      task->execute();
      #ifndef DT_DISABLE
        progress::manager->check_interrupts_main();
      #endif
    }
    catch (...) {
      idle_job_->catch_exception();
    }
  }
}


size_t ThreadWorker::get_index() const noexcept {
  return thread_index_;
}


void ThreadWorker::assign_job(ThreadJob* job) noexcept {
  job_ = job;
}




}  // namespace dt
