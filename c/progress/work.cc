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
#include "progress/manager.h"       // dt::progress::manager
#include "progress/progress_bar.h"  // dt::progress::progress_bar
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
    pbar(nullptr)
{
  dt::progress::manager.start_work(this);
  // progress manager will call this->init();
}

work::~work() {
  dt::progress::manager.finish_work(this);
}

void work::init(progress_bar* pb, work* parent) {
  xassert(pb);
  pbar = pb;
  if (parent) {
    pmin = parent->calculate_progress(parent->done_amount);
    pmax = parent->calculate_progress(parent->done_tentative);
  }
}



void work::add_work_amount(size_t amount) {
  total_amount += amount;
  push_to_progress_bar();
}

void work::set_done_amount(size_t amount) {
  done_amount = amount;
  done_tentative = amount;
  push_to_progress_bar();
}

void work::add_done_amount(size_t amount) {
  done_amount += amount;
  done_tentative = done_amount;
  push_to_progress_bar();
}

void work::add_tentative_amount(size_t amount) {
  done_tentative += amount;
  push_to_progress_bar();
}



void work::set_status(Status s) {
  pbar->set_status(s);
}

void work::set_message(std::string message) {
  pbar->set_message(std::move(message));
}




//-------- Private -------------------------------------------------------------

double work::calculate_progress(size_t amount) const {
  double progress = static_cast<double>(amount) /
                    static_cast<double>(total_amount);
  return pmin + (pmax - pmin) * progress;
}

void work::push_to_progress_bar() const {
  xassert(done_amount <= done_tentative && done_tentative <= total_amount);
  xassert(pbar);
  pbar->set_progress(calculate_progress(done_amount),
                     calculate_progress(done_tentative));
}



}} // namespace dt::progress
