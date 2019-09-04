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
#include "progress/progress_bar.h"     // dt::progress::progress_bar
#include "progress/progress_manager.h" // dt::progress::progress_manager
#include "progress/work.h"
#include "utils/assert.h"
namespace dt {
namespace progress {


work::work(size_t amount)
  : total_amount(amount),
    done_amount(0),
    done_tentative(0),
    pmin(0.0),
    pmax(1.0),
    pbar(nullptr),
    message_set(false)
{
  dt::progress::manager->start_work(this);
  // progress manager will call this->init();
}

work::~work() {
 if (!pbar) return;
 try {
   dt::progress::manager->finish_work(this, false);
 } catch (...) {};
}


void work::init(progress_bar* pb, work* parent) {
  xassert(pb);
  pbar = pb;
  if (parent) {
    pmin = parent->calculate_progress(parent->done_amount);
    pmax = parent->calculate_progress(parent->done_tentative);
  }
}


/**
 * This method must be called at the end of progress reporting.
 */
void work::done() {
  xassert(done_amount == total_amount);
  dt::progress::manager->finish_work(this, true);
  pbar = nullptr;
}


void work::add_work_amount(size_t amount) noexcept {
  total_amount += amount;
  push_to_progress_bar();
}


void work::set_done_amount(size_t amount) noexcept {
  done_amount = amount;
  done_tentative = amount;
  push_to_progress_bar();
}


void work::add_done_amount(size_t amount) noexcept {
  done_amount += amount;
  done_tentative = done_amount;
  push_to_progress_bar();
}


size_t work::get_done_amount() noexcept {
  return done_amount;
}


void work::add_tentative_amount(size_t amount) noexcept {
  done_tentative += amount;
  push_to_progress_bar();
}


void work::set_message(std::string message) {
  pbar->set_message(std::move(message));
  message_set = true;
}




//-------- Private -------------------------------------------------------------

double work::calculate_progress(size_t amount) const noexcept {
  double progress = static_cast<double>(amount) /
                    static_cast<double>(total_amount);
  return pmin + (pmax - pmin) * progress;
}

void work::push_to_progress_bar() const noexcept {
  wassert(done_amount <= done_tentative && done_tentative <= total_amount);
  wassert(pbar);
  pbar->set_progress(calculate_progress(done_amount),
                     calculate_progress(done_tentative));
}



//------------------------------------------------------------------------------
// subtask
//------------------------------------------------------------------------------

subtask::subtask(work& w, size_t amount) noexcept
  : parent(w), work_amount(amount)
{
  parent.add_tentative_amount(work_amount);
}


void subtask::done() {
  parent.add_done_amount(work_amount);
}


}} // namespace dt::progress
