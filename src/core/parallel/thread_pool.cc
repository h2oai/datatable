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
#include <thread>      // std::thread::hardware_concurrency
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_team.h"
#include "parallel/thread_worker.h"
#include "progress/progress_manager.h"
#include "python/_all.h"
#include "python/arg.h"
#include "utils/assert.h"
#include "options.h"
#include "utils/macros.h"
#if !DT_OS_WINDOWS
  #include <pthread.h>   // pthread_atfork
#endif
namespace dt {


// Singleton instance of the ThreadPool
ThreadPool* thpool = new ThreadPool;

// no fork on Windows
#if !DT_OS_WINDOWS

  static void _prepare_fork() {
    size_t n = thpool->size();
    // Put the threadpool into an uninitialized state, shutting down all
    // workers and the monitor thread.
    thpool->resize(0);
    // The monitor thread must be shut down last, because the resize() operation
    // will attempt to initialize it if absent.
    // thpool->monitor = nullptr;
    // Resizing the thread pool back to `n` does not create any threads: we
    // postpone thread initialization until they are actually needed.
    thpool->resize(n);
  }

  static void _child_cleanup_after_fork() {
    // Replace the current thread pool instance with a new one, ensuring that all
    // schedulers and workers have new mutexes/condition variables.
    // The previous value of `thpool` is abandoned without deleting since that
    // memory is owned by the parent process.
    size_t n = thpool->size();
    thpool = new ThreadPool;
    progress::manager = new progress::progress_manager;
    thpool->resize(n);
  }

  static bool fork_handlers_registered = false;

#endif



//------------------------------------------------------------------------------
// ThreadPool
//------------------------------------------------------------------------------

ThreadPool::ThreadPool()
  : num_threads_requested(0),
    current_team(nullptr)
{
  #if !DT_OS_WINDOWS
    // no fork on Windows
    if (!fork_handlers_registered) {
      pthread_atfork(_prepare_fork, nullptr, _child_cleanup_after_fork);
      fork_handlers_registered = true;
    }
  #endif
}




size_t ThreadPool::size() const noexcept {
  return num_threads_requested;
}

void ThreadPool::resize(size_t n) {
  num_threads_requested = n;
  // Adjust the actual thread count, but only if the threads were already
  // instantiated.
  if (!workers_.empty()) {
    instantiate_threads();
  }
}

void ThreadPool::instantiate_threads() {
  size_t n = num_threads_requested;
  // init_monitor_thread();
  if (workers_.size() == n) return;
  if (workers_.size() < n) {
    workers_.reserve(n);
    for (size_t i = workers_.size(); i < n; ++i) {
      try {
        auto worker = new ThreadWorker(i, &controller);
        workers_.push_back(worker);
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
    for (size_t i = n; i < workers_.size(); ++i) {
      delete workers_[i];
    }
    workers_.resize(n);
  }
  xassert(workers_.size() == num_threads_requested);
}


// void ThreadPool::init_monitor_thread() noexcept {
  // if (!monitor) {
  //   monitor = std::unique_ptr<monitor_thread>(
  //               new monitor_thread(&controller)
  //             );
  // }
// }


void ThreadPool::execute_job(thread_scheduler* job) {
  xassert(current_team);
  if (workers_.empty()) instantiate_threads();
  controller.awaken_and_run(job, workers_.size());
  controller.join();
  // careful: workers_.size() may not be equal to num_threads_requested during
  // shutdown
}


bool ThreadPool::in_parallel_region() const noexcept {
  return (current_team != nullptr);
}

size_t ThreadPool::n_threads_in_team() const noexcept {
  return current_team? current_team->size() : 0;
}


thread_team* ThreadPool::get_team_unchecked() noexcept {
  return thpool->current_team;
}


// void ThreadPool::enable_monitor(bool a) noexcept {
//   (void) a;
//   // init_monitor_thread();
//   // monitor->set_active(a);
// }




//------------------------------------------------------------------------------
// Monitor thread control.
//------------------------------------------------------------------------------
// void enable_monitor(bool a) noexcept {
//   thpool->enable_monitor(a);
// }



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


// std::mutex& python_mutex() {
//   return thpool->python_mutex;
// }


std::mutex& team_mutex() {
  return thpool->global_mutex;
}


void ThreadPool::init_options() {
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
