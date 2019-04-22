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
#ifndef dt_PARALLEL_PROGRESS_h
#define dt_PARALLEL_PROGRESS_h
#include <memory>
namespace dt {
namespace progress {


enum class Status : int8_t {
  RUNNING = 0,
  FINISHED = 1,
  ERROR = 2,
  CANCELLED = 3,
};


class progress_bar;

class work {
  private:
    size_t total_amount;
    size_t done_amount;
    size_t tentative_amount;
    double pmin, pmax;
    // borrowed ref, lifetime guaranteed by `progress_manager` holding a
    // reference to this class
    progress_bar* pbar;

  public:
    explicit work(size_t amount);
    ~work();

    void add_work(size_t amount);
    void start_task(size_t amount);

    void set_progress(size_t amount);
    void add_progress(size_t amount);
    void set_status(Status s);
    void set_message(std::string message);
    size_t get_progress() const;

  private:
    void update_progress_bar() const;
};


class subtask {
  private:
    work& parent;
    size_t work_amount;

  public:
    subtask(work& w, size_t amount) : parent(w), work_amount(amount) {
      parent.start_task(work_amount);
    }
    ~subtask() {
      parent.add_progress(work_amount);
    }
};


void init_options();

}}  // namespace dt::progress
#endif
