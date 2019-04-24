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
#include <chrono>      // std::chrono
#include <sstream>     // std::stringstream
#include <string>      // std::string
#include "progress/common.h"
#include "python/_all.h"
namespace dt {
namespace progress {



class progress_bar {
  using ptime_t = std::chrono::steady_clock::time_point;
  using dtime_t = std::chrono::steady_clock::duration;
  using rtime_t = std::chrono::duration<double>;  // in seconds
  private:
    // progress bar state
    double progress;           // [0.0 .. 1.0]
    double tentative_progress; // [progress .. 1.0]
    std::string message;
    Status status;
    size_t : 56;

    // parameters (constant during progress bar's lifetime)
    int bar_width;
    bool enabled;
    bool clear_on_success;
    bool use_colors;
    bool use_unicode;

    // runtime support
    dtime_t update_interval;
    ptime_t time_started;
    ptime_t time_next_update;
    py::oobj pyfn_write;
    py::oobj pyfn_flush;
    py::oobj pyfn_external;
    py::otuple py_args;
    bool visible;
    bool force_redraw;
    size_t : 48;

  public:
    progress_bar();

    void set_progress(double actual, double tentative);
    void set_status(Status);
    void set_status_finished();
    void set_message(std::string&&);

    void refresh();

  private:
    void _check_interrupts();
    void _report_to_python();
    void _render_to_stdout();
    void _render_percentage(std::stringstream& out);
    void _render_progressbar_unicode(std::stringstream& out);
    void _render_progressbar_ascii(std::stringstream& out);
    void _render_message(std::stringstream& out);
};



}} // namespace dt::progress
#endif
