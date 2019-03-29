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



thread_scheduler::~thread_scheduler() {}


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

void thread_shutdown_scheduler::init(
    size_t nnew, size_t nold, thread_sleep_scheduler* sch)
{
  xassert(nold > nnew);
  n_threads_to_keep = nnew;
  n_threads_to_kill = nold - nnew;
  sleep_scheduler = sch;
}


thread_task* thread_shutdown_scheduler::get_next_task(size_t thread_index) {
  if (thread_index < n_threads_to_keep) {
    return nullptr;  // thread goes back to sleep
  }
  n_threads_to_kill--;
  sleep_scheduler->pretend_thread_went_to_sleep();
  return &shutdown;
}




//------------------------------------------------------------------------------
// thread sleep scheduler
//------------------------------------------------------------------------------

thread_task* thread_sleep_scheduler::get_next_task(size_t) {
  return &tsleep[index];
}


void thread_sleep_scheduler::awaken(thread_scheduler* next) {
  size_t i = index;
  size_t j = (i + 1) % N_SLEEP_TASKS;  // next value for `index`
  {
    std::lock_guard<std::mutex> lock(tsleep[i].mutex);
    tsleep[i].next_scheduler = next;
    tsleep[j].next_scheduler = nullptr;
    tsleep[j].n_threads_sleeping = 0;
    index = j;
  }
  // Unlock mutex before awaking all sleeping threads
  tsleep[i].alarm.notify_all();
}


// Wait until all threads go back to sleep (which would mean the job is done)
void thread_sleep_scheduler::join(size_t nthreads) {
  thread_sleep_task& st = tsleep[index];
  size_t n_sleeping = 0;
  while (n_sleeping < nthreads) {
    std::this_thread::yield();
    std::unique_lock<std::mutex> lock(st.mutex);
    n_sleeping = st.n_threads_sleeping;
  }

  if (saved_exception) {
    std::rethrow_exception(saved_exception);
  }
}


void thread_sleep_scheduler::pretend_thread_went_to_sleep() {
  std::lock_guard<std::mutex> lock(tsleep[index].mutex);
  tsleep[index].n_threads_sleeping++;
}




//------------------------------------------------------------------------------
// once scheduler
//------------------------------------------------------------------------------

once_scheduler::once_scheduler(size_t nth, thread_task* task_)
  : done(nth, 0),
    task(task_) {}


thread_task* once_scheduler::get_next_task(size_t i) {
  if (done[i].v) {
    return nullptr;
  }
  done[i].v = 1;
  return task;
}




void run_once_per_thread(function<void(size_t)> f) {
  thread_pool& thpool = get_thread_pool();
  simple_task task(f);
  once_scheduler sch(thpool.size(), &task);
  thpool.execute_job(sch);
}



}  // namespace dt
