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
#include <algorithm>                // std::max
#include "progress/_options.h"
#include "progress/progress_bar.h"
#include "python/string.h"          // py::ostring
#include "utils/assert.h"
#include "utils/terminal/terminal.h"
#include "options.h"                // dt::get_option
namespace dt {
namespace progress {

static PyObject* status_strings[4];

static void init_status_strings() {
  if (status_strings[0]) return;
  status_strings[int(Status::RUNNING)]   = py::ostring("running").release();
  status_strings[int(Status::FINISHED)]  = py::ostring("finished").release();
  status_strings[int(Status::ERROR)]     = py::ostring("error").release();
  status_strings[int(Status::CANCELLED)] = py::ostring("cancelled").release();
}


progress_bar::~progress_bar() {}


void progress_bar_disabled::set_progress(double, double) noexcept {}
void progress_bar_disabled::set_status_finished() {}
void progress_bar_disabled::set_status_error(bool) {}
void progress_bar_disabled::set_message(std::string&&) {}
void progress_bar_disabled::refresh() {}


progress_bar_enabled::progress_bar_enabled() {
  // Progress bar's state
  progress = 0.0;
  tentative_progress = 0.0;
  status = Status::RUNNING;

  // Parameters
  bar_width = 50;
  clear_on_success = dt::progress::clear_on_success;
  use_colors = dt::Terminal::standard_terminal().colors_enabled();
  use_unicode = dt::Terminal::standard_terminal().unicode_allowed();

  // Runtime support
  rtime_t freq { 1.0/updates_per_second };
  update_interval = std::chrono::duration_cast<dtime_t>(freq);
  time_started = std::chrono::steady_clock::now();
  time_next_update = time_started + update_interval;

  if (dt::progress::progress_fn) {
    init_status_strings();
    pyfn_external = py::oobj(dt::progress::progress_fn);
  }
  else {
    auto rstdout = py::rstdout();
    pyfn_write = rstdout.get_attr("write");
    pyfn_flush = rstdout.get_attr("flush");
  }

  visible = false;
  force_redraw = false;
}


void progress_bar_enabled::set_progress(double actual, double tentative) noexcept {
  wassert(0.0 <= actual && actual <= tentative && tentative <= 1.0);
  progress = actual;
  tentative_progress = tentative;
}

void progress_bar_enabled::set_status_finished() {
  wassert(status == Status::RUNNING);
  status = Status::FINISHED;
  force_redraw = true;
  refresh();
}

void progress_bar_enabled::set_status_error(bool cancelled) {
  wassert(status == Status::RUNNING);
  status = cancelled? Status::CANCELLED : Status::ERROR;
  force_redraw = true;
  refresh();
}


void progress_bar_enabled::set_message(std::string&& msg) {
  message = std::move(msg);
  force_redraw = true;
}


// When determining whether to display progress bar or not, we first
// estimate the future duration of the task, and then compare it versus the
// `min_duration` option. However, if the current progress is above 50%,
// then we gradually increase the requirement for displaying the progress
// bar, up to `2 * min_duration`.
//
// This is done in order to avoid the situation where the estimated
// duration reaches the `min_duration` threshold near the end of the
// calculation, causing the progress bar to appear briefly near the end.
// Although such situation is ultimately unavoidable, the approach that we
// use should greately reduce its likelihood. This is because typically the
// curve estimated_duration(progress) will be increasing but convex, whereas
// our function threshold_duration(progress) is concave. Thus, these two
// curves are much likelier to intersect at low levels of `progress`, than
// at high levels.
//
void progress_bar_enabled::refresh() {
  auto now = std::chrono::steady_clock::now();

  if (!visible) {
    auto tpassed = std::chrono::duration_cast<rtime_t>(now - time_started);
    double estimated_duration = tpassed.count() / std::max(progress, 0.1);
    double threshold_duration = min_duration * std::max(1.0, 2*progress);
    if (estimated_duration < threshold_duration) return;
    visible = true;
    force_redraw = true;
  }

  if (force_redraw || now >= time_next_update) {
    time_next_update = now + update_interval;
    if (pyfn_external)
      _report_to_python();
    else
      _render_to_stdout();
  }
}



//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void progress_bar_enabled::_report_to_python() {
  static py::onamedtupletype ntt(
    "ProgressStatus",
    "Progress state for dt.options.progress.callback function",
    {{"progress", "The percentage of job done, a number between 0 and 1"},
     {"status", "One of: 'running', 'finished', 'error' or 'cancelled'"},
     {"message", "General description of what is currently being done"}}
  );
  py::onamedtuple nt(ntt);
  nt.set(0, py::ofloat(progress));
  nt.set(1, py::oobj(status_strings[static_cast<int>(status)]));
  nt.set(2, py::ostring(message));
  py::otuple arg(1);
  arg.set(0, std::move(nt));
  pyfn_external.call(std::move(arg));
}


void progress_bar_enabled::_render_to_stdout() {
  std::stringstream out;

  if (visible) out << '\r';
  _render_percentage(out);
  if (use_unicode) _render_progressbar_unicode(out);
  else             _render_progressbar_ascii(out);
  _render_message(out);

  pyfn_write.call(py::otuple{ py::ostring(out.str()) });
  pyfn_flush.call();
}


void progress_bar_enabled::_render_percentage(std::stringstream& out) {
  int percentage = static_cast<int>(progress * 100 + 0.1);
  if (percentage < 10) out << ' ';
  if (percentage < 100) out << ' ';
  out << percentage << "% ";
}


void progress_bar_enabled::_render_progressbar_unicode(std::stringstream& out) {
  double x = progress * bar_width;
  int n_full_chars = static_cast<int>(x + 0.001);
  int fractional_char = static_cast<int>((x - n_full_chars) * 8);
  int n_empty_chars = bar_width - n_full_chars - (fractional_char? 1 : 0);
  if (use_colors) out << "\x1B[2m";
  out << '|';
  for (int i = 0; i < n_full_chars; ++i) {
    out << "\xE2\x96\x88";
  }
  if (fractional_char) {
    out << "\xE2\x96" << static_cast<char>('\x90' - fractional_char);
  }
  for (int i = 0; i < n_empty_chars; ++i) {
    out << ' ';
  }
  out << '|';
  if (use_colors) out << "\x1B[m";
}


void progress_bar_enabled::_render_progressbar_ascii(std::stringstream& out) {
  int n_chars = static_cast<int>(progress * bar_width + 0.001);
  int i = 0;
  if (use_colors) out << "\x1B[2m";
  out << '[';
  for (; i < n_chars; ++i) out << '#';
  for (; i < bar_width; ++i) out << ' ';
  out << ']';
  if (use_colors) out << "\x1B[m";
}


void progress_bar_enabled::_render_message(std::stringstream& out) {
  out << ' ' << message << ' ';
  switch (status) {
    case Status::RUNNING:
      out << "\x1B[K"; // clear till the end of the terminal line
      return;

    case Status::FINISHED:
      if (clear_on_success) {
        out.str("");  // clear the content generated thus far in `out`
        out << "\x1B[1G\x1B[K";
        return;
      }
      if (use_colors) out << "\x1B[1;32m";  // bold-green
      out << "[done]";
      break;

    case Status::ERROR:
      if (use_colors) out << "\x1B[1;31m";  // bold-red
      out << "[error]";
      break;

    case Status::CANCELLED:
      if (use_colors) out << "\x1B[1;33m";  // bold-ochra
      out << "[cancelled]";
      break;
  }
  if (use_colors) out << "\x1B[m";
  out << "\x1B[K"; // clear till the end of the terminal line
  out << '\n';
}


}}  // namespace dt::progress
