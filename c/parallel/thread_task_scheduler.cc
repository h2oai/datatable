//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <thread>              // std::this_thread
#include "parallel/thread_pool.h"
#include "parallel/thread_task_scheduler.h"
#include "parallel/thread_worker.h"
#include "utils/assert.h"
namespace dt {


task::~task() {}

thread_task_scheduler::~thread_task_scheduler() {}



//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

void shutdown_thread_task::execute(thread_worker* worker) {
  worker->set_scheduler(nullptr);
}

void put_to_sleep_task::execute(thread_worker* worker) {
  worker->set_scheduler(sleep_scheduler);
}



void thread_shutdown_scheduler::init(
    size_t nnew, size_t nold, thread_task_scheduler* sleep_scheduler)
{
  xassert(nold > nnew);
  n_threads_to_keep = nnew;
  n_threads_to_kill = nold - nnew;
  lullaby.sleep_scheduler = sleep_scheduler;
}


task* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep) {
    return &lullaby;
  }
  n_threads_to_kill--;
  return &shutdown;
}

void thread_shutdown_scheduler::join() {
  while (n_threads_to_kill)
    std::this_thread::yield();
}



//------------------------------------------------------------------------------
// thread sleep scheduler
//------------------------------------------------------------------------------

void thread_sleep_task::execute(thread_worker* worker) {
  std::unique_lock<std::mutex> lock(mutex);
  while (true) {
    // Wait for the `alarm` condition variable to be notified, but may also
    // wake up spuriously, in which case we check `sleeping` flag to decide
    // whether we need to keep waiting or not.
    alarm.wait(lock);
    if (next_scheduler) break;
  }
  worker->set_scheduler(next_scheduler);
}



task* thread_sleep_scheduler::get_next_task(size_t) {
  return &sleep;
}

void thread_sleep_scheduler::join() {
  // This is the only non-joinable task. Don't do anything here.
}

void thread_sleep_scheduler::awaken(thread_task_scheduler* next) {
  {
    std::lock_guard<std::mutex> lock(sleep.mutex);
    sleep.next_scheduler = next;
  }  // Unlock `mutex` before awaking all sleeping threads
  sleep.alarm.notify_all();
}


}  // namespace dt
