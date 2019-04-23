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
#include <Python.h>
#include "progress/common.h"
#include "progress/progress_bar.h"
#include "python/string.h"          // py::ostring
namespace dt {
namespace progress {



progress_bar::progress_bar() {
  // Progress bar's state
  progress = 0.0;
  tentative_progress = 0.0;
  status = Status::RUNNING;

  // Parameters
  bar_width = 50;
  enabled = dt::progress::enabled;
  clear_on_success = true;
  if (enabled) {
    use_colors = dt::get_option("display.use_colors").to_bool_strict();
    use_unicode = dt::get_option("display.allow_unicode").to_bool_strict();
  }

  // Runtime support
  rtime_t freq { 1.0/updates_per_second };
  update_interval = std::chrono::duration_cast<dtime_t>(freq);
  time_started = std::chrono::steady_clock::now();
  time_next_update = time_started + update_interval;
  if (enabled) {
    if (dt::progress::progress_fn) {
      pyfn_external = py::oobj(dt::progress::progress_fn);
      py_args = py::otuple(3);
    }
    else {
      auto stdout = py::stdout();
      pyfn_write = stdout.get_attr("write");
      pyfn_flush = stdout.get_attr("flush");
      py_args = py::otuple(1);
    }
  }
  visible = false;
  force_redraw = false;
}


void progress_bar::set_progress(double actual, double tentative) {
  xassert(0.0 <= actual && actual <= tentative && tentative <= 1.0);
  progress = actual;
  tentative_progress = tentative;
}

void progress_bar::set_status(Status status_) {
  if (status == status_) return;
  status = status_;
  force_redraw = true;
}

void progress_bar::set_message(std::string&& msg) {
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
void progress_bar::refresh() {
  if (!enabled) return _check_interrupts();

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


void progress_bar::_check_interrupts() {
  int ret = PyErr_CheckSignals();
  if (ret) throw PyError();
}


void progress_bar::_report_to_python() {
  PyObject* status_pyobj = status_strings[static_cast<int>(status)];
  py_args.replace(0, py::ofloat(progress));
  py_args.replace(1, py::oobj(status_pyobj));
  py_args.replace(2, py::ostring(message));
  progress_fn.call(py_args);
}


void progress_bar::_render_to_stdout() {
  std::stringstream out;

  if (visible) out << '\r';
  _render_percentage(out);
  if (use_unicode) _render_progressbar_unicode(out);
  else             _render_progressbar_ascii(out);
  _render_message(out);

  py_args.replace(0, py::ostring(out.str()));
  pyfn_write.call(py_args);
  pyfn_flush.call();
}


void progress_bar::_render_percentage(std::stringstream& out) {
  int percentage = static_cast<int>(progress * 100 + 0.1);
  out << percentage << "% ";
  if (percentage < 10) out << ' ';
  if (percentage < 100) out << ' ';
}


void progress_bar::_render_progressbar_unicode(std::stringstream& out) {
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


void progress_bar::_render_progressbar_ascii(std::stringstream& out) {
  int n_chars = static_cast<int>(progress * bar_width + 0.001);
  int i = 0;
  if (use_colors) out << "\x1B[2m";
  out << '[';
  for (; i < n_chars; ++i) out << '#';
  for (; i < bar_width; ++i) out << ' ';
  out << ']';
  if (use_colors) out << "\x1B[m";
}


void progress_bar::_render_message(std::stringstream& out) {
  out << ' ';
  switch (status) {
    case Status::RUNNING:
      out << message;
      return;

    case Status::FINISHED:
      if (clear_on_success) {
        out.str("");  // clear the percentage / progress bar
        out << "\x1B[1G\x1B[K";
        return;
      }
      out << message;
      break;

    case Status::ERROR:
      if (use_colors) out << "\x1B[1;31m";  // bold-red
      out << "(error)";
      break;

    case Status::CANCELLED:
      if (use_colors) out << "\x1B[1;33m";  // bold-ochra
      out << "(cancelled)";
      break;
  }
  if (use_colors) out << "\x1B[m";
  out << '\n';
}



}}  // namespace dt::progress
