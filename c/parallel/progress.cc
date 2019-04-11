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
#include "parallel/progress.h"
namespace dt {
namespace progress {


//------------------------------------------------------------------------------
// work
//------------------------------------------------------------------------------

work::work(double amount, work* parent_work) {
  xassert(amount > 0);
  total_amount = amount;
  done_amount = 0;
  subtask_amount = 0;
  parent = parent_work;
  if (parent) {
    multiplier = parent->multiplier * parent->subtask_amount / amount;
    parent_progress = parent->get_progress();
  } else {
    multiplier = 1 / amount;
    parent_progress = 0;
  }
}


work::~work() {
  if (parent) parent->done();  // ?
}


void work::done(double amount) {
  xassert(amount >= 0);
  done_amount += amount;
  xassert(done_amount <= total_amount * 1.00001);
}


double work::get_progress() const {
  return parent_progress + done_amount * multiplier;
}



}}  // namespace dt::progress
