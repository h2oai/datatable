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
#include "progress/manager.h"
namespace dt {
namespace progress {


// Static instance
progress_manager manager;

progress_manager::progress_manager()
  : pb(nullptr) {}




void progress_manager::start_work(work* task) {
  xassert((pb == nullptr) == tasks.empty());
  if (pb) {
    work* previous_work = tasks.top();
    previous_work->subtask(task);
  } else {
    pb = new progress_bar;
  }
  tasks.push(task);
}

void progress_manager::finish_work(work* task) {
  xassert(!tasks.empty() && tasks.top() == task);
  tasks.pop();
  if (tasks.empty()) {
    delete pb;
    pb = nullptr;
  }
}



}} // namespace dt::progress
