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
#include <thread>      // std::thread::hardware_concurrency
#include <pthread.h>   // pthread_atfork
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_team.h"
#include "parallel/thread_worker.h"
#include "utils/assert.h"
namespace dt {



//------------------------------------------------------------------------------
// thread_pool
//------------------------------------------------------------------------------

thread_pool::thread_pool()
  : num_threads_requested(0),
    current_team(nullptr) {}

// In the current implementation the thread_pool instance never gets deleted
// thread_pool::~thread_pool() {
//   resize(0);
// }


size_t thread_pool::size() const noexcept {
  return num_threads_requested;
}

void thread_pool::resize(size_t n) {
  num_threads_requested = n;
  if (!workers.empty()) {
    resize_impl();
  }
}

void thread_pool::resize_impl() {
  size_t n = num_threads_requested;
  if (workers.size() == n) return;
  if (workers.size() < n) {
    workers.reserve(n);
    for (size_t i = workers.size(); i < n; ++i) {
      workers.push_back(new thread_worker(i, &controller));
    }
    // Wait until all threads are properly alive & safely asleep
    controller.join(n);
  }
  else {
    thread_team tt(n, this);
    thread_shutdown_scheduler tss(n, &controller);
    execute_job(&tss);
    for (size_t i = n; i < workers.size(); ++i) {
      delete workers[i];
    }
    workers.resize(n);
  }
}


void thread_pool::execute_job(thread_scheduler* job) {
  xassert(in_master_thread());
  xassert(current_team);
  if (workers.empty()) resize_impl();
  controller.awaken_and_run(job);
  controller.join(workers.size());
  // careful: workers.size() may not be equal to num_threads_requested during
  // shutdown
}


bool thread_pool::in_master_thread() const noexcept {
  return this_thread_index() == size_t(-1);
}

bool thread_pool::in_parallel_region() const noexcept {
  return (current_team != nullptr);
}

size_t thread_pool::n_threads_in_team() const noexcept {
  return current_team? current_team->size() : 0;
}




//------------------------------------------------------------------------------
// thread_pool static instance
//------------------------------------------------------------------------------

// Singleton instance of the thread_pool, returned by
// `thread_pool::get_instance()`.
static thread_pool* _instance = nullptr;

void _child_cleanup_after_fork() {
  if (!_instance) return;
  size_t n = _instance->size();
  _instance = new thread_pool;
  _instance->resize(n);
}

thread_pool* thread_pool::get_instance() {
  if (!_instance) {
    // Replace the current thread pool instance with a new one, ensuring that all
    // schedulers and workers have new mutexes/condition variables.
    // The previous value of `_instance` is abandoned without deleting since that
    // memory is owned by the parent process.
    _instance = new thread_pool;
    pthread_atfork(nullptr, nullptr, _child_cleanup_after_fork);
  }
  return _instance;
}

thread_pool* thread_pool::get_instance_unchecked() noexcept {
  return _instance;
}

thread_team* thread_pool::get_team_unchecked() noexcept {
  return _instance->current_team;
}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

size_t num_threads_in_pool() {
  return thread_pool::get_instance()->size();
}

size_t num_threads_in_team() {
  return _instance? _instance->n_threads_in_team() : 0;
}


static thread_local size_t thread_index = size_t(-1);
size_t this_thread_index() {
  return thread_index;
}
void _set_thread_num(size_t i) {
  thread_index = i;
}


size_t get_hardware_concurrency() noexcept {
  unsigned int nth = std::thread::hardware_concurrency();
  if (!nth) nth = 1;
  return static_cast<size_t>(nth);
}



}  // namespace dt
