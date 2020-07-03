//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "parallel/api.h"           // dt::this_thread_index
#include "progress/_options.h"
#include "progress/progress_bar.h"
#include "progress/progress_manager.h"
#include "progress/work.h"
#include "utils/assert.h"
namespace dt {
namespace progress {


// Static instance; it will be re-initialized when forking -- see
// `parallel/thread_pool.cc::_child_cleanup_after_fork()`
progress_manager* manager = new progress_manager;



progress_manager::progress_manager()
  : pbar(nullptr)
{
  interrupt_status = InterruptStatus::RUN;
}


void progress_manager::start_work(work* task) {
  xassert((pbar == nullptr) == tasks.empty());
  if (tasks.empty()) {
    std::lock_guard<std::mutex> lock(mutex);
    if (dt::progress::enabled) {
      pbar = new progress_bar_enabled();
    } else {
      pbar = new progress_bar_disabled();
    }

    task->init(pbar, nullptr);
  } else {
    work* previous_work = tasks.top();
    task->init(pbar, previous_work);
  }
  tasks.push(task);
}


void progress_manager::finish_work(work* task, bool successfully) {
  (void) task;
  xassert(!tasks.empty() && tasks.top() == task);
  xassert(pbar != nullptr);
  tasks.pop();

  std::lock_guard<std::mutex> lock(mutex);
  if (tasks.empty()) {
    if (successfully) pbar->set_status_finished();
    else              pbar->set_status_error(false);
    delete pbar;
    pbar = nullptr;
  }
  interrupt_status = InterruptStatus::RUN;
}


void progress_manager::update_view() const {
  xassert(dt::this_thread_index() == size_t(-1) ||
          dt::num_threads_in_team() == 0);
  std::lock_guard<std::mutex> lock(mutex);

  // Handle interrupt if in a parallel region.
  // If not in a parallel region, `handle_interrupt()`
  // must be invoked in a special way, i.e.
  // when `is_interrupt_occurred() == true`.
  if (dt::num_threads_in_team()) handle_interrupt();
  if (pbar) pbar->refresh();
}



void progress_manager::set_interrupt() const noexcept {
  interrupt_status = InterruptStatus::HANDLE_INTERRUPT;
}


bool progress_manager::is_interrupt_occurred() const noexcept {
  return interrupt_status != InterruptStatus::RUN;
}


void progress_manager::reset_interrupt_status() const noexcept {
  interrupt_status = InterruptStatus::RUN;
}


void progress_manager::handle_interrupt() const {
  if (interrupt_status == InterruptStatus::HANDLE_INTERRUPT) {
    if (pbar) pbar->set_status_error(true);
    interrupt_status = InterruptStatus::ABORT_EXECUTION;
    PyErr_SetNone(PyExc_KeyboardInterrupt);
    throw PyError();
  }
}


void progress_manager::set_status_cancelled() noexcept {
  try {
    if (pbar) pbar->set_status_error(true);
  } catch (...) {}
}


void progress_manager::check_interrupts_main() {
  xassert(dt::this_thread_index() == 0);
  // std::lock_guard<std::recursive_mutex> lock(dt::python_mutex());
  PythonLock pylock;

  if (PyErr_CheckSignals()) {
    if (pbar) pbar->set_status_error(true);
    // Do not set interrupt status here, otherwise another thread
    // might see that status and throw its own exception before this
    // one has a chance of getting processed.
    // Instead, the interrupt status will be set in
    // `Job_Idle::catch_exception()`
    throw PyError();
  }
  if (pbar) {
    pbar->refresh();
  }
}




}} // namespace dt::progress
