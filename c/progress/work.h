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
#ifndef dt_PROGRESS_WORK_h
#define dt_PROGRESS_WORK_h
#include "progress/common.h"
namespace dt {
namespace progress {



class work {
  private:
    size_t total_amount;
    size_t done_amount;
    size_t done_tentative;
    double pmin, pmax;
    // borrowed ref, lifetime guaranteed by `progress_manager` holding a
    // reference to this class
    progress_bar* pbar;

  public:
    explicit work(size_t amount);
    ~work();
    void init(progress_bar* pb, work* parent);

    void add_work_amount(size_t);
    void set_done_amount(size_t);
    void add_done_amount(size_t);
    void add_tentative_amount(size_t amount);

    void set_status(Status s);
    void set_message(std::string message);

  private:
    double calculate_progress(size_t amount) const;
    void push_to_progress_bar() const;
};



class subtask {
  private:
    work& parent;
    size_t work_amount;

  public:
    subtask(work& w, size_t amount) : parent(w), work_amount(amount) {
      parent.add_tentative_amount(work_amount);
    }
    ~subtask() {
      parent.add_done_amount(work_amount);
    }
};



}}  // namespace dt::progress
#endif
