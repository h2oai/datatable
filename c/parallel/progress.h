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
namespace dt {
namespace progress {


class work {
  private:
    double total_amount;
    double done_amount;
    double subtask_amount;

    work* parent;
    double parent_progress;
    double multiplier;

  public:
    explicit work(double amount, work* parent_work);
    ~work();

    void done(double amount);
    double get_progress() const;
};


class task {
  public:
    explicit task(double amount);
    ~task();
}




}}  // namespace dt::progress
#endif
