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
namespace dt {
namespace progress {

// forward-declare
class progress_bar;
class work;


/**
 * Singleton class that acts as a liaison between the progress bar and
 * multiple `work` instances.
 *
 * manager.update_view()
 *
 */
class progress_manager {
  private:
    progress_bar* pb;
    std::stack<work*> tasks;

  public:
    void update_view() const;

  public:  // package-private
    progress_manager();
    void start_work(work* task);
    void finish_work(work* task);
};


extern progress_manager manager;


}} // dt::progress
#endif
