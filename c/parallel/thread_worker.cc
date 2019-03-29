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
#include <thread>   // std::thread
#include "parallel/thread_scheduler.h"
#include "parallel/thread_worker.h"
namespace dt {


/**
 * The worker creates its own thread of execution, and then detaches it. The
 * thread will be executing function `thread_worker::run()` continuously. The
 * only way to shut down the thread is to cause the `run()` function to stop
 * its loop.
 */
thread_worker::thread_worker(size_t i, thread_scheduler* ts)
  : thread_index(i),
    current_scheduler(ts),
    sleep_scheduler(ts)
{
  std::thread t(&thread_worker::run, this);
  t.detach();
}


/**
 * This is the main function that will be run within the thread. It
 * continuously picks up tasks from the thread pool and executes them. This
 * function stops running (terminating the thread) once `current_scheduler`
 * becomes nullptr.
 *
 * If the task returned from the scheduler is nullptr, then the thread worker
 * switches to
 */
void thread_worker::run() noexcept {
  while (current_scheduler) {
    try {
      thread_task* task = current_scheduler->get_next_task(thread_index);
      if (task) {
        task->execute(this);
      } else {
        current_scheduler = sleep_scheduler;
      }
    } catch (...) {
      current_scheduler->handle_exception();
    }
  }
}

size_t thread_worker::get_index() const noexcept {
  return thread_index;
}




//------------------------------------------------------------------------------
// Worker control via tasks
//------------------------------------------------------------------------------

void shutdown_thread_task::execute(thread_worker* worker) {
  worker->current_scheduler = nullptr;
}


void thread_sleep_task::execute(thread_worker* worker) {
  std::unique_lock<std::mutex> lock(mutex);
  while (true) {
    // Wait for the `alarm` condition variable to be notified, but may also
    // wake up spuriously, in which case we check `sleeping` flag to decide
    // whether we need to keep waiting or not.
    alarm.wait(lock);
    if (next_scheduler) break;
  }
  worker->current_scheduler = next_scheduler;
}


}  // namespace dt
