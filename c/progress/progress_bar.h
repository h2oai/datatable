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
#ifndef dt_PROGRESS_PROGRESSBAR_h
#define dt_PROGRESS_PROGRESSBAR_h
#include <chrono>         // std::chrono
#include <sstream>        // std::stringstream
#include <string>         // std::string
#include "python/_all.h"  // py::oobj
namespace dt {
namespace progress {


enum class Status : size_t {
  RUNNING = 0,
  FINISHED = 1,
  ERROR = 2,
  CANCELLED = 3,
};


/**
 * Base progress bar class declaring progress bar API.
 */
class progress_bar {
  public:
    virtual ~progress_bar();
    virtual void set_progress(double actual, double tentative) noexcept = 0;
    virtual void set_status_finished() = 0;
    virtual void set_status_error(bool cancelled) = 0;
    virtual void set_message(std::string&& msg) = 0;
    virtual void refresh() = 0;
};


/**
 * Progress bar stub to be used when progress bar is not enabled.
 */
class progress_bar_disabled : public progress_bar {
  public:
    void set_progress(double actual, double tentative) noexcept;
    void set_status_finished();
    void set_status_error(bool cancelled);
    void set_message(std::string&& msg);
    void refresh();
};


/**
 * Actual progress bar implementation.
 */
class progress_bar_enabled : public progress_bar {
  using ptime_t = std::chrono::steady_clock::time_point;
  using dtime_t = std::chrono::steady_clock::duration;
  using rtime_t = std::chrono::duration<double>;  // in seconds
  private:
    // progress bar state
    double progress;           // [0.0 .. 1.0]
    double tentative_progress; // [progress .. 1.0]
    std::string message;
    Status status;

    // parameters (constant during progress bar's lifetime)
    int bar_width;
    bool clear_on_success;
    bool use_colors;
    bool use_unicode;
    size_t : 8;

    // runtime support
    dtime_t update_interval;
    ptime_t time_started;
    ptime_t time_next_update;
    py::oobj pyfn_write;
    py::oobj pyfn_flush;
    py::oobj pyfn_external;
    bool visible;
    bool force_redraw;
    size_t : 48;

  public:
    progress_bar_enabled();

    void set_progress(double actual, double tentative) noexcept;
    void set_status_finished();
    void set_status_error(bool cancelled);
    void set_message(std::string&& msg);

    void refresh();

  private:
    void _report_to_python();
    void _render_to_stdout();
    void _render_percentage(std::stringstream& out);
    void _render_progressbar_unicode(std::stringstream& out);
    void _render_progressbar_ascii(std::stringstream& out);
    void _render_message(std::stringstream& out);
};


}} // namespace dt::progress
#endif
