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
#include "parallel/thread_worker.h"
#include "utils/assert.h"
#include "utils/c+++.h"
namespace dt {



//------------------------------------------------------------------------------
// thread_pool
//------------------------------------------------------------------------------

thread_pool* thread_pool::_instance = nullptr;

thread_pool* thread_pool::get_instance() {
  if (!_instance) _instance = new thread_pool;
  return _instance;
}



thread_pool::thread_pool()
  : num_threads_requested(0),
    master_thread_id(std::this_thread::get_id())
{
  // pthread_atfork callback needs to be established within the main process
  // only, which is indicated by `_instance` being nullptr during construction.
  if (!_instance) {
    pthread_atfork(nullptr, nullptr, []{
      thread_pool::get_instance()->cleanup_after_fork();
    });
  }
}

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
      workers.push_back(
        make_unique<dt::thread_worker>(i, &controller));
    }
    // Wait until all threads are properly alive & safely asleep
    controller.join(n);
  }
  else {
    thread_shutdown_scheduler tss(n, &controller);
    execute_job(&tss);
    workers.resize(n);
  }
}


void thread_pool::execute_job(thread_scheduler* job) {
  if (workers.empty()) resize_impl();
  controller.awaken_and_run(job);
  controller.join(workers.size());
  // careful: workers.size() may not be equal to num_threads_requested during
  // shutdown
}


// This function should be called in the child process after the fork only.
void thread_pool::cleanup_after_fork() {
  xassert(this == _instance);
  // Replace the current thread pool instance with a new one, ensuring that all
  // schedulers and workers have new mutexes/condition variables
  _instance = new thread_pool;
  _instance->resize(num_threads_requested);

  // Abandon the current instance (`this`) without deleting, since it is owned
  // by the parent process anyways.
}




//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------

size_t get_num_threads() {
  return thread_pool::get_instance()->size();
}


size_t get_hardware_concurrency() noexcept {
  unsigned int nth = std::thread::hardware_concurrency();
  if (!nth) nth = 1;
  return static_cast<size_t>(nth);
}



}  // namespace dt
