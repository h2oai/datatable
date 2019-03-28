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
#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <thread>              // std::this_thread
#include "parallel/thread_pool.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_worker.h"
#include "utils/assert.h"
namespace dt {


thread_task::~thread_task() {}

thread_scheduler::~thread_scheduler() {}


void thread_scheduler::join() {
  wait_until_finish();
  if (saved_exception) {
    std::rethrow_exception(saved_exception);
  }
}


void thread_scheduler::handle_exception() noexcept {
  try {
    {
      std::lock_guard<std::mutex> lock(mtx);
      saved_exception = std::current_exception();
    }
    abort_execution();
  } catch (...) {}
}


void thread_scheduler::abort_execution() {
  // default implementation does nothing
}




//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

void thread_shutdown_scheduler::init(size_t nnew, size_t nold) {
  xassert(nold > nnew);
  n_threads_to_keep = nnew;
  n_threads_to_kill = nold - nnew;
}


thread_task* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep) {
    return nullptr;  // thread goes back to sleep
  }
  n_threads_to_kill--;
  return &shutdown;
}


void thread_shutdown_scheduler::wait_until_finish() {
  while (n_threads_to_kill)
    std::this_thread::yield();
}




//------------------------------------------------------------------------------
// thread sleep scheduler
//------------------------------------------------------------------------------

thread_task* thread_sleep_scheduler::get_next_task(size_t) {
  return &sleep;
}

void thread_sleep_scheduler::wait_until_finish() {
  // This is the only non-joinable task. Don't do anything here.
}

void thread_sleep_scheduler::awaken(thread_scheduler* next) {
  {
    std::lock_guard<std::mutex> lock(sleep.mutex);
    sleep.next_scheduler = next;
  }  // Unlock `mutex` before awaking all sleeping threads
  sleep.alarm.notify_all();
}




//------------------------------------------------------------------------------
// once scheduler
//------------------------------------------------------------------------------

once_scheduler::once_scheduler(size_t nth, thread_task* task_)
  : done(nth, 0),
    num_working_threads(static_cast<unsigned int>(nth)),
    task(task_) {}


thread_task* once_scheduler::get_next_task(size_t i) {
  if (done[i].v) {
    num_working_threads--;
    return nullptr;
  } else {
    done[i].v = 1;
    return task;
  }
}


void once_scheduler::wait_until_finish() {
  while (num_working_threads)
    std::this_thread::yield();
}


void run_once_per_thread(function<void()> f) {
  thread_pool& thpool = get_thread_pool();
  simple_task task(f);
  once_scheduler sch(thpool.size(), &task);
  thpool.execute_job(sch);
}



}  // namespace dt
