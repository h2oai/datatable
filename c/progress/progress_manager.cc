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
  : pbar(nullptr), caught_interrupt(false)
{
  abort_execution.store(false);
}


void progress_manager::start_work(work* task) {
  if (tasks.empty()) {
    std::lock_guard<std::mutex> lock(mutex);
    xassert(pbar == nullptr);
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
  handle_interrupt();
  if (successfully && tasks.empty()) {
    pbar->set_status_finished();
    delete pbar;
    pbar = nullptr;
  }
  abort_execution.store(false);
}


void progress_manager::update_view() const {
  xassert(dt::this_thread_index() == size_t(-1));
  std::lock_guard<std::mutex> lock(mutex);
  handle_interrupt();
  if (pbar) pbar->refresh();
}


void progress_manager::set_error_status(bool cancelled) noexcept {
  std::lock_guard<std::mutex> lock(mutex);
  if (!pbar) return;
  try {
    pbar->set_status_error(cancelled);
  } catch (...) {}
  delete pbar;
  pbar = nullptr;
}


void progress_manager::set_interrupt() const {
  caught_interrupt = true;
  abort_execution.store(true);
  // PyErr_SetInterrupt();
}


bool progress_manager::get_abort_execution() const {
  return abort_execution;
}


void progress_manager::reset_abort_execution() const {
  abort_execution.store(false);
}


void progress_manager::handle_interrupt() const {
  if (!caught_interrupt) return;
  caught_interrupt = false;
  PyErr_SetNone(PyExc_KeyboardInterrupt);
  throw PyError();
}


}} // namespace dt::progress
