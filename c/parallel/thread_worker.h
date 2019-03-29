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
#ifndef dt_PARALLEL_THREAD_WORKER_h
#define dt_PARALLEL_THREAD_WORKER_h
#include <condition_variable>   // std::condition_variable
#include <cstddef>              // std::size_t
#include <mutex>                // std::mutex
#include "parallel/thread_task.h"
namespace dt {
using std::size_t;

// forward-declare
class thread_scheduler;



/**
 * A class that encapsulates thread-specific runtime information. After
 * instantiation, we expect this class to be accessed within its own thread
 * only. This makes it safe to have variables such as `scheduler` non-atomic.
 *
 * Any communication with the worker (including changing to a new scheduler)
 * is performed only via the current scheduler: the scheduler may emit a task
 * that changes the worker's state.
 *
 * The thread stops running when `scheduler` becomes nullptr.
 */
class thread_worker {
  friend struct shutdown_thread_task;
  friend struct thread_sleep_task;

  private:
    const size_t thread_index;
    thread_scheduler* current_scheduler;
    thread_scheduler* sleep_scheduler;

  public:
    thread_worker(size_t i, thread_scheduler* ts);
    thread_worker(const thread_worker&) = default;
    thread_worker(thread_worker&&) = default;

    void run() noexcept;
    size_t get_index() const noexcept;
};




struct shutdown_thread_task : public thread_task {
  void execute(thread_worker* worker) override;
};


struct thread_sleep_task : public thread_task {
  std::mutex mutex;
  std::condition_variable alarm;
  thread_scheduler* next_scheduler = nullptr;

  void execute(thread_worker* worker) override;
};




}  // namespace dt
#endif
