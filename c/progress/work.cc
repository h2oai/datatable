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
#include "progress/progress_manager.h"  // dt::progress::manager
#include "progress/work.h"
namespace dt {
namespace progress {



work::work(size_t amount)
  : total_amount(amount),
    done_amount(0),
    tentative_amount(0),
    pmin(0.0),
    pmax(1.0),
    pbar(nullptr)
{
  dt::progress::manager.start_work(this);
}

work::~work() {
  dt::progress::manager.finish_work(this);
}


void work::add_work(size_t amount) {
  total_amount += amount;
  tentative_amount = amount;
  push_to_progress_bar();
}

void work::start_task(size_t amount) {
  tentative_amount = amount;
  push_to_progress_bar();
}


void work::set_progress(size_t amount) {
  done_amount = amount;
  tentative_amount = 0;
  push_to_progress_bar();
}

void work::add_progress(size_t amount) {
  done_amount += amount;
  tentative_amount = 0;
  push_to_progress_bar();
}


void work::set_status(Status s) {
  xassert(pbar);
  pbar->set_status(s);
}

void work::set_message(std::string message) {
  xassert(pbar);
  pbar->set_message(std::move(message));
}


void work::push_to_progress_bar() const {
  xassert(amount + tentative_amount <= total_amount);
  xassert(pbar);
  double progress1 = 1.0 * done_amount / total_amount;
  double progress2 = 1.0 * (done_amount + tentative_amount) / total_amount;
  pbar->set_progress(pmin + (pmax - pmin) * progress1,
                     pmin + (pmax - pmin) * progress2);
}



}} // namespace dt::progress
