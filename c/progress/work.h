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
#include <string>   // std::string
namespace dt {
namespace progress {

// forward-declare
class progress_bar;
class work;


/**
 * Main user-facing class to support progress display. This class
 * represents a progress state of a certain user-defined activity,
 * such as task, job, process, loop, etc. The user is expected to
 * create an instance of this class whenever any long-running task
 * is executed, then update the progress during the execution, and
 * then call `work.done()` in the end.
 *
 * Example:
 *
 *     dt::progress::work task(n);
 *     task.set_message("Processing...");
 *     for (size_t i = 0; i < n; ++i) {
 *       ... do something ...
 *       task.add_done_amount(1);
 *     }
 *     task.done();
 *
 * The constructor `work(n)` takes a single argument -- the amount of
 * work that the user plans to do. The user is allowed to increast
 * the initial work allocation later (if new work comes in
 * unexpectedly), although this is better avoided as it results in
 * progress bar jumping back.
 *
 * The following functions can be used to submit progress reports
 * to the `work` object:
 *
 *   - `set_done_amount(a)`: set the amount of work done so far to `a`,
 *     where `a` must be less than or equal to the initial `n`;
 *
 *   - `add_done_amount(a)`: increment the amount of work done by `a`,
 *     the total work done should not exceed `n`;
 *
 *   - `add_tentative_amount(a)`: mark a certain portion of work as
 *     "tentative": you declare that you start performing some work in
 *     the amount of `a`, but will be unable to generate accurate
 *     progress reports during that time. Using either
 *     `set_done_amount()` or `add_done_amount()` later will clear any
 *     tentative work.
 *     This can be used within a task whose progress state cannot be
 *     assessed, such as reading from a pipe. In addition, this
 *     function is used to create nested `work` objects: whenever a
 *     new `work` is created while there is another `work` pending,
 *     will assume as its responsibility to track progress for the
 *     duration of the tentative work amount of the parent.
 *
 * At the end of `work`'s lifetime the user is expected to call
 * `work.done()`. This will check whether the amount of work reported
 * is the same as was planned, and update the progress bar. The `work`
 * instance must not be used after this call.
 *
 * The reason we require explicit `work.done()` (as opposed to a
 * traditional destructor) is that we want to distinguish the work
 * being completed normally, and the work being shut down due to an
 * exception. When an exception is thrown, we want the progress bar
 * to remain alive until the point where the exception is caught and
 * proper status indicator can be set on the progress bar. This
 * usually happens at the C++/Python boundary.
 *
 * [NOTE]: This class is not thread-safe. The user must ensure that
 * no more than one thread can update this class at a time.
 */
class work {
  private:
    size_t total_amount;
    size_t done_amount;
    size_t done_tentative;
    double pmin, pmax;
    progress_bar* pbar;  // borrowed ref
    bool message_set;
    size_t : 56;

  public:
    explicit work(size_t amount);
    void set_message(std::string message);
    void done();
    ~work();

    void add_work_amount(size_t) noexcept;
    void set_done_amount(size_t) noexcept;
    size_t get_done_amount() noexcept;
    void add_done_amount(size_t) noexcept;
    void add_tentative_amount(size_t) noexcept;

  public: // package-private
    // used from progress_manager to complete initialization of the class
    void init(progress_bar* pb, work* parent);

  private:
    double calculate_progress(size_t amount) const noexcept;
    void push_to_progress_bar() const noexcept;
};



class subtask {
  private:
    work& parent;
    size_t work_amount;

  public:
    subtask(work& w, size_t amount) noexcept;
    void done();
};



}}  // namespace dt::progress
#endif
