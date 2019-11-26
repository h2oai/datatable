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
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_team.h"
#include "parallel/thread_worker.h"
#include "progress/progress_manager.h"
#include "python/_all.h"
#include "utils/assert.h"
#include "options.h"
#include "utils/macros.h"
#if !DT_OS_WINDOWS
  #include <pthread.h>   // pthread_atfork
#endif


namespace dt {


// Singleton instance of the thread_pool
thread_pool* thpool = new thread_pool;

static void _child_cleanup_after_fork() {
  // Replace the current thread pool instance with a new one, ensuring that all
  // schedulers and workers have new mutexes/condition variables.
  // The previous value of `thpool` is abandoned without deleting since that
  // memory is owned by the parent process.
  size_t n = thpool->size();
  thpool = new thread_pool;
  progress::manager = new progress::progress_manager;
  thpool->resize(n);
}



//------------------------------------------------------------------------------
// thread_pool
//------------------------------------------------------------------------------
#if !DT_OS_WINDOWS
  // no fork on Windows
  static bool after_fork_handler_registered = false;
#endif

thread_pool::thread_pool()
  : num_threads_requested(0),
    current_team(nullptr)
{
#if !DT_OS_WINDOWS
  // no fork on Windows
  if (!after_fork_handler_registered) {
    pthread_atfork(nullptr, nullptr, _child_cleanup_after_fork);
    after_fork_handler_registered = true;
  }
#endif
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
  // Adjust the actual thread count, but only if the threads were already
  // instantiated.
  if (!workers.empty()) {
    instantiate_threads();
  }
}

void thread_pool::instantiate_threads() {
  size_t n = num_threads_requested;
  init_monitor_thread();
  if (workers.size() == n) return;
  if (workers.size() < n) {
    workers.reserve(n);
    for (size_t i = workers.size(); i < n; ++i) {
      try {
        auto worker = new thread_worker(i, &controller);
        workers.push_back(worker);
      } catch (...) {
        // If threads cannot be created (for example if user has requested
        // too many threads), then stop creating new workers and use as
        // many as we were able to create thus far.
        n = num_threads_requested = i;
      }
    }
    // Wait until all threads are properly alive & safely asleep
    controller.join();
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
  xassert(workers.size() == num_threads_requested);
}


void thread_pool::init_monitor_thread() noexcept {
  if (!monitor) {
    monitor = std::unique_ptr<monitor_thread>(
                new monitor_thread(&controller)
              );
  }
}


void thread_pool::execute_job(thread_scheduler* job) {
  xassert(current_team);
  if (workers.empty()) instantiate_threads();
  controller.awaken_and_run(job, workers.size());
  controller.join();
  // careful: workers.size() may not be equal to num_threads_requested during
  // shutdown
}


bool thread_pool::in_parallel_region() const noexcept {
  return (current_team != nullptr);
}

size_t thread_pool::n_threads_in_team() const noexcept {
  return current_team? current_team->size() : 0;
}


thread_team* thread_pool::get_team_unchecked() noexcept {
  return thpool->current_team;
}


void thread_pool::enable_monitor(bool a) noexcept {
  init_monitor_thread();
  monitor->set_active(a);
}


bool thread_pool::is_monitor_enabled() noexcept {
  return monitor? monitor->get_active() : false;
}


//------------------------------------------------------------------------------
// Monitor thread control.
//------------------------------------------------------------------------------
void enable_monitor(bool a) noexcept {
  thpool->enable_monitor(a);
}

bool is_monitor_enabled() noexcept {
  return thpool->is_monitor_enabled();
}


//------------------------------------------------------------------------------
// Misc
//------------------------------------------------------------------------------
size_t num_threads_in_pool() {
  return thpool->size();
}

size_t num_threads_in_team() {
  return thpool->n_threads_in_team();
}

size_t num_threads_available() {
  auto tt = thpool->get_team_unchecked();
  return tt? tt->size() : thpool->size();
}

static thread_local size_t thread_index = 0;
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


std::mutex& python_mutex() {
  return thpool->monitor->mutex;
}


void thread_pool::init_options() {
  // By default, set the number of threads to `hardware_concurrency`
  thpool->resize(get_hardware_concurrency());

  dt::register_option(
    "nthreads",

    /* getter= */[]() -> py::oobj {
      return py::oint(num_threads_in_pool());
    },

    /* setter= */[](const py::Arg& value) {
      int32_t nth = value.to_int32_strict();
      if (nth <= 0) nth += static_cast<int32_t>(get_hardware_concurrency());
      if (nth <= 0) nth = 1;
      thpool->resize(static_cast<size_t>(nth));
    },

    "The number of threads used by datatable internally.\n"
    "\n"
    "Many calculations in `datatable` module are parallelized. This \n"
    "setting controls how many threads will be used during such\n"
    "calculations.\n"
    "\n"
    "Initially, this option is set to the value returned by C++ call\n"
    "`std::thread::hardware_concurrency()`. This is usually equal to the\n"
    "number of available cores.\n"
    "\n"
    "You can set `nthreads` to a value greater or smaller than the\n"
    "initial setting. For example, setting `nthreads = 1` will force the\n"
    "library into a single-threaded mode. Setting `nthreads` to 0 will\n"
    "restore the initial value equal to the number of processor cores.\n"
    "Setting `nthreads` to a value less than 0 is equivalent to\n"
    "requesting that fewer threads than the maximum.\n");
}


}  // namespace dt
