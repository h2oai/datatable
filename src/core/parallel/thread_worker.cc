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
#include "parallel/thread_worker.h"
#include "progress/progress_manager.h"  // dt::progress::progress_manager
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "parallel/api.h"
#include "parallel/thread_pool.h"
namespace dt {



/**
  * The worker creates its own thread of execution. The thread will be
  * executing function `ThreadWorker::run()` continuously. The only
  * way to shut down the thread is to cause the `run()` function to
  * stop its loop.
  */
ThreadWorker::ThreadWorker(size_t i, Job_Idle* wc)
  : thread_index(i),
    scheduler(wc),
    controller(wc)
{
  if (i == 0) {
    wc->set_master_worker(this);
    scheduler = nullptr;
    _set_thread_num(0);
  } else {
    // Create actual execution thread only when `this` is fully initialized
    wc->on_before_thread_added();
    thread = std::thread(&ThreadWorker::run, this);
  }
}


ThreadWorker::~ThreadWorker() {
  if (thread.joinable()) thread.join();
}



/**
 * This is the main function that will be run within the thread. It
 * continuously picks up tasks from the scheduler and executes them. This
 * function stops running (terminating the thread) once `scheduler` becomes
 * nullptr.
 *
 * If the task returned from the scheduler is nullptr, then the thread worker
 * switches to "sleep" scheduler and waits until it is awaken by the condition
 * variable inside the sleep task.
 */
void ThreadWorker::run() noexcept {
  _set_thread_num(thread_index);
  while (scheduler) {
    try {
      ThreadTask* task = scheduler->get_next_task(thread_index);
      if (task) {
        task->execute();
      } else {
        scheduler = controller;
      }
    } catch (...) {
      // enable_monitor(false);
      controller->catch_exception();
      scheduler->abort_execution();
    }
  }
}


/**
 * Similar to run(), but designed to run from the master thread. The
 * differences are following:
 *   - this method does NOT run continuously, instead it starts with
 *     a new job, and finishes when the job is done.
 *   - the `scheduler` is not used (since it is never set by the
 *     controller), instead the `job` is passed explicitly.
 */
void ThreadWorker::run_master(ThreadJob* job) noexcept {
  if (!job) return;
  while (true) {
    try {
      ThreadTask* task = job->get_next_task(0);
      if (!task) break;
      task->execute();
      progress::manager->check_interrupts_main();
    } catch (...) {
      // enable_monitor(false);
      controller->catch_exception();
      job->abort_execution();
    }
  }
}


size_t ThreadWorker::get_index() const noexcept {
  return thread_index;
}


void ThreadWorker::assign_job(ThreadJob* job) noexcept {
  scheduler = job;
}




}  // namespace dt
