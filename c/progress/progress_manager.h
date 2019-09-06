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
#include <stack>      // std::stack
#include <mutex>      // std::mutex, std::lock_guard
namespace dt {
namespace progress {

// forward-declare
class progress_bar;
class progress_bar_disabled;
class progress_bar_enabled;
class work;


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

    // This flag is initially set to `false`. When SIGINT signal is caught,
    // `caught_interrupt` is set to `true`. When progress manager handles
    // the interrupt, it also resets `caught_interrupt` back to `false`.
    mutable bool caught_interrupt;

    // This flag is kind of similar to the above, but is atomic and not
    // cleared by the interrupt handler. It is only cleared when threads are
    // joining, and used in the cases when there is no special scheduler
    // (like in the cases of `parallel_for_static`, `nested_for_static`
    // or a single threaded `parallel_for_ordered`) but we still want
    // to abort execution of all threads when SIGINT is received.
    mutable std::atomic<bool> abort_execution;
    size_t : 48;

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
    bool get_abort_execution() const;
    void reset_abort_execution() const;
    void handle_interrupt() const;
};


extern progress_manager* manager;


}} // dt::progress
#endif
