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
    double total_amount;
    double done_amount;
    double subtask_amount;

    work* parent;
    std::unique_ptr<progress_bar> pbar;
    double parent_progress;
    double multiplier;

  public:
    explicit work(double amount);
    ~work();

    void set_progress(double p);
    void set_status(Status s);
    void set_message(std::string message);
    double get_progress() const;

    void update_progress_bar();
};


class task {
  public:
    explicit task(double amount);
    ~task();
};


void init_options();
work* current_progress();

}}  // namespace dt::progress
#endif
