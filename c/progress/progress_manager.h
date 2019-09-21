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
#ifndef dt_PROGRESS_MANAGER_h
#define dt_PROGRESS_MANAGER_h
#include <atomic>
#include <stack>      // std::stack
#include <mutex>      // std::mutex, std::lock_guard
namespace dt {
namespace progress {

// forward-declare
class progress_bar;
class progress_bar_disabled;
class progress_bar_enabled;
class work;


enum class InterruptStatus : unsigned char {
  RUN = 0,
  ABORT_EXECUTION = 1,
  HANDLE_INTERRUPT = 2
};

/**
 * Singleton class that acts as a liaison between the progress bar and
 * multiple `work` instances.
 *
 * manager.update_view()
 *     This function should be called periodically from the master
 *     thread to re-draw the progress bar (if needed) and check
 *     for signals. This function may throw an exception if there
 *     is a KeyboardInterrupt signal pending, or if the progress-
 *     reporting function raised an exception in python.
 */
class progress_manager {
  private:
    // Owned reference. This class creates a progress_bar instance when
    // the first task is pushed onto the stack. It destroyes the instance
    // when the last task is popped off the stack. This cycle continues,
    // while more and more top-level tasks are received.
    progress_bar* pbar;
    std::stack<work*> tasks;

    // This mutex is used to protect access to `pbar`. In particular,
    // `finish_work()` method can delete `pbar` from one thread
    // during finalization, while at the same time another thread
    // can start doing `update_view()` or `set_error_status()`
    // also accessing `pbar`. Without the mutex protection such an access
    // can result in a segfault.
    mutable std::mutex mutex;

    // This flag is initially set to InterruptStatus::RUN, meaning that
    // threads are allowed to perform their jobs. When SIGINT signal is caught,
    // it is set to InterruptStatus::HANDLE_INTERRUPT, meaning that
    // job execution should be aborted and interrupt should be handled.
    // When interrupt is handled in `handle_interrupt()`, this flag
    // is set to InterruptStatus::ABORT_EXECUTION, meaning that
    // job execution should be aborted. When execution is aborted,
    // this flag is set back to InterruptStatus::RUN.
    mutable std::atomic<InterruptStatus> interrupt_status;
    size_t : 56;

  public:
    void update_view() const;
    void set_error_status(bool cancelled) noexcept;

  public:  // package-private
    progress_manager();
    // called by a new `work` object when it is constructed
    void start_work(work* task);
    // called by `work.done()` / `~work()`
    void finish_work(work* task, bool successfully);
    void set_interrupt() const;
    bool is_interrupt_occurred() const;
    void reset_interrupt_status() const;
    void handle_interrupt() const;
};


extern progress_manager* manager;


}} // dt::progress
#endif
