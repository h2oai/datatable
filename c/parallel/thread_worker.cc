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
#include "parallel/thread_worker.h"
#include "progress/manager.h"  // dt::progress::manager
#include "utils/exceptions.h"
namespace dt {


//------------------------------------------------------------------------------
// thread worker
//------------------------------------------------------------------------------

/**
 * The worker creates its own thread of execution. The thread will be executing
 * function `thread_worker::run()` continuously. The only way to shut down the
 * thread is to cause the `run()` function to stop its loop.
 */
thread_worker::thread_worker(size_t i, idle_job* wc)
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
    thread = std::thread(&thread_worker::run, this);
  }
}


thread_worker::~thread_worker() {
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
void thread_worker::run() noexcept {
  _set_thread_num(thread_index);
  while (scheduler) {
    try {
      thread_task* task = scheduler->get_next_task(thread_index);
      if (task) {
        task->execute(this);
      } else {
        scheduler = controller;
      }
    } catch (...) {
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
void thread_worker::run_master(thread_scheduler* job) noexcept {
  if (!job) return;
  while (true) {
    try {
      thread_task* task = job->get_next_task(0);
      if (!task) break;
      task->execute(this);
    } catch (...) {
      controller->catch_exception();
      job->abort_execution();
    }
  }
}


size_t thread_worker::get_index() const noexcept {
  return thread_index;
}




//------------------------------------------------------------------------------
// "worker controller" scheduler
//------------------------------------------------------------------------------

idle_job::sleep_task::sleep_task(idle_job* ij)
  : controller(ij), next_scheduler(nullptr) {}

void idle_job::sleep_task::execute(thread_worker* worker) {
  std::unique_lock<std::mutex> lock(controller->mutex);
  controller->n_threads_running--;
  while (!next_scheduler) {
    // Wait for the `wakeup_all_threads_cv` condition variable to be notified,
    // but may also wake up spuriously, in which case we check `next_scheduler`
    // to decide whether we need to keep waiting or not.
    controller->wakeup_all_threads_cv.wait(lock);
  }
  worker->scheduler = next_scheduler;
}


idle_job::idle_job() {
  curr_sleep_task = new sleep_task(this);
  prev_sleep_task = new sleep_task(this);
  n_threads_running = 0;
  monitor = std::unique_ptr<monitor_thread>(new monitor_thread(this));
}


thread_task* idle_job::get_next_task(size_t) {
  return curr_sleep_task;
}


void idle_job::awaken_and_run(thread_scheduler* job, size_t nthreads) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    curr_sleep_task->next_scheduler = job;
    prev_sleep_task->next_scheduler = nullptr;
    std::swap(curr_sleep_task, prev_sleep_task);
    saved_exception = nullptr;
    n_threads_running = nthreads - 1; // master never sleeps!
  }
  // Unlock mutex before awaking all sleeping threads
  wakeup_all_threads_cv.notify_all();
  monitor->set_active(true);
  master_worker->run_master(job);
}


// Wait until all threads go back to sleep (which would mean the job is done)
void idle_job::join() {
  while (true) {
    std::unique_lock<std::mutex> lock(mutex);
    if (n_threads_running == 0) break;
  }

  // Clear `.next_scheduler` flag of the previous sleep task, indicating that
  // we no longer run in a parallel region (see `is_running()`).
  prev_sleep_task->next_scheduler = nullptr;
  monitor->set_active(false);

  if (saved_exception) {
    std::rethrow_exception(saved_exception);
  }
}


void idle_job::set_master_worker(thread_worker* worker) noexcept {
  master_worker = worker;
}

void idle_job::on_before_thread_removed() {
  std::lock_guard<std::mutex> lock(mutex);
  n_threads_running--;
}

void idle_job::on_before_thread_added() {
  std::lock_guard<std::mutex> lock(mutex);
  n_threads_running++;
}


void idle_job::catch_exception() noexcept {
  try {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!saved_exception) {
        saved_exception = std::current_exception();
      }
    }
    auto current_job = prev_sleep_task->next_scheduler;
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

void thread_shutdown_scheduler::shutdown_task::execute(thread_worker* worker) {
  worker->scheduler = nullptr;
}


thread_shutdown_scheduler::thread_shutdown_scheduler(
    size_t nnew, idle_job* sch)
  : n_threads_to_keep(nnew),
    controller(sch) {}


thread_task* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep) {
    return nullptr;  // thread goes back to sleep
  }
  controller->on_before_thread_removed();
  return &shutdown;
}



}  // namespace dt
