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


//------------------------------------------------------------------------------
// thread worker
//------------------------------------------------------------------------------

/**
  * The worker creates its own thread of execution. The thread will be
  * executing function `ThreadWorker::run()` continuously. The only
  * way to shut down the thread is to cause the `run()` function to
  * stop its loop.
  */
ThreadWorker::ThreadWorker(size_t i, idle_job* wc)
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




//------------------------------------------------------------------------------
// "worker controller" scheduler
//------------------------------------------------------------------------------

idle_job::sleep_task::sleep_task(idle_job* ij)
  : controller(ij), next_scheduler{nullptr} {}

void idle_job::sleep_task::execute() {
  controller->n_threads_running--;
  semaphore.wait();
  xassert(next_scheduler);
  thpool->assign_job_to_current_thread(next_scheduler);
}


idle_job::idle_job() {
  curr_sleep_task = new sleep_task(this);
  prev_sleep_task = new sleep_task(this);
  n_threads_running = 0;
}


ThreadTask* idle_job::get_next_task(size_t) {
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
 * may call arbitrary API of `idle_job`.
 *
 * Note that we set the variable `n_threads_running` explicitly here
 * (as opposed to, say, allowing each thread to increment this counter
 * upon awaking). This is necessary, because we want to prevent the
 * situation where the OS would delay waking up the threads, so that
 * by the time we run `join()` the number of running threads would be
 * zero, even though no work has been done yet.
 */
void idle_job::awaken_and_run(ThreadJob* job, size_t nthreads) {
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
void idle_job::join() {
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


void idle_job::set_master_worker(ThreadWorker* worker) noexcept {
  master_worker = worker;
}


void idle_job::on_before_thread_removed() {
  xassert(n_threads_running > 0);
  n_threads_running--;
}

void idle_job::on_before_thread_added() {
  n_threads_running++;
}


// Multiple threads may throw exceptions simultaneously, thus
// need to protect access to `saved_exception` with a mutex.
// In addition, `job->abort_execution()` is also protected,
// just in case, ensuring that only one thread can call that
// method at a time.
//
void idle_job::catch_exception() noexcept {
  try {
    std::lock_guard<std::mutex> lock(mutex);
    if (!saved_exception) {
      saved_exception = std::current_exception();
    }
    ThreadJob* current_job = prev_sleep_task->next_scheduler;
    if (current_job) {
      current_job->abort_execution();
    }
  } catch (...) {}
}


bool idle_job::is_running() const noexcept {
  return (prev_sleep_task->next_scheduler != nullptr);
}




//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

void thread_shutdown_scheduler::shutdown_task::execute() {
  thpool->assign_job_to_current_thread(nullptr);
}


thread_shutdown_scheduler::thread_shutdown_scheduler(
    size_t nnew, idle_job* sch)
  : n_threads_to_keep(nnew),
    controller(sch) {}


ThreadTask* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep || thread_index == 0) {
    return nullptr;  // thread goes back to sleep
  }
  controller->on_before_thread_removed();
  return &shutdown;
}



}  // namespace dt
