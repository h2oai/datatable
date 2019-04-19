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
#include <chrono>      // std::chrono
#include <sstream>     // std::stringstream
#include <string>      // std::string
#include <Python.h>
#include "models/py_validator.h"
#include "parallel/progress.h"
#include "python/_all.h"          // py::oobj, py::ofloat
#include "python/string.h"        // py::ostring
#include "options.h"              // dt::register_option
namespace dt {
namespace progress {


static work* current_work = nullptr;


//------------------------------------------------------------------------------
// options
//------------------------------------------------------------------------------

static double updates_per_second = 25.0;
static double min_duration = 0.5;
static py::oobj progress_fn = nullptr;
static bool disabled = false;

static PyObject* status_strings[4];

static bool stdout_is_a_terminal() {
  py::robj stdout(PySys_GetObject("stdout"));
  if (stdout.is_none()) return false;
  py::oobj isatty = stdout.get_attrx("isatty");
  if (!isatty) return false;
  py::oobj res = isatty.call();
  return res.is_bool()? res.to_bool_strict() : false;
}

void init_options() {
  disabled = !stdout_is_a_terminal();

  dt::register_option(
    "progress.enabled",
    []{ return py::obool(!disabled); },
    [](py::oobj value){ disabled = !value.to_bool_strict(); },
    "If False, then all progress reporting functionality will be turned off."
  );

  dt::register_option(
    "progress.updates_per_second",
    []{
      return py::ofloat(updates_per_second);
    },
    [](py::oobj value){
      double x = value.to_double();
      py::Validator::check_positive(x, value);
      updates_per_second = x;
    },
    "How often should the display of the progress bar be updated."
  );

  dt::register_option(
    "progress.min_duration",
    []{
      return py::ofloat(min_duration);
    },
    [](py::oobj value){
      double x = value.to_double();
      py::Validator::check_not_negative(x, value);
      min_duration = x;
    },
    "Do not show progress bar if the duration of an operation is\n"
    "smaller than this value. If this setting is non-zero, then\n"
    "the progress bar will only be shown for long-running operations,\n"
    "whose duration (estimated or actual) exceeds this threshold."
  );

  dt::register_option(
    "progress.callback",
    []{
      return progress_fn? progress_fn : py::None();
    },
    [](py::oobj value){
      progress_fn = value.is_none()? nullptr : value;
    },
    "If None, then the builtin progress-reporting function will be used.\n"
    "Otherwise, this value specifies a function or object to be called\n"
    "at each progress event.\n"
    "\n"
    "The function is expected to have the following signature:\n"
    "\n"
    "    fn(progress, status, message)\n"
    "\n"
    "where `progress` is a float in the range 0.0 .. 1.0; `status` is a\n"
    "string, one of 'running', 'finished', 'error' or 'cancelled'; and\n"
    "`message` is a custom string describing the operation currently\n"
    "being performed."
  );

  status_strings[0] = py::ostring("running").release();
  status_strings[1] = py::ostring("finished").release();
  status_strings[2] = py::ostring("error").release();
  status_strings[3] = py::ostring("cancelled").release();
}




//------------------------------------------------------------------------------
// progress bar
//------------------------------------------------------------------------------

class progress_bar {
  using ptime_t = std::chrono::steady_clock::time_point;
  using dtime_t = std::chrono::steady_clock::duration;
  using rtime_t = std::chrono::duration<double>;  // in seconds
  private:
    py::oobj outfile;
    dtime_t update_interval;
    ptime_t time_start;
    ptime_t time_next_update;
    std::string message;
    double progress;  // [0.0 .. 1.0]
    int bar_width;
    bool visible;
    bool clear_on_success;
    bool use_colors;
    bool use_unicode;
    Status status;
    size_t : 56;

  public:
    progress_bar()
      : outfile(PySys_GetObject("stdout")),
        update_interval(std::chrono::duration_cast<dtime_t>(
                            rtime_t(1.0/updates_per_second))),
        time_start(std::chrono::steady_clock::now()),
        time_next_update(time_start + update_interval),
        progress(0.0),
        bar_width(50),
        visible(false),
        clear_on_success(true),
        use_colors(dt::get_option("display.use_colors").to_bool_strict()),
        use_unicode(dt::get_option("display.allow_unicode").to_bool_strict()),
        status(Status::RUNNING) {}

    void set_progress(double progress_) {
      xassert(progress_ >= 0 && progress_ <= 1.0);
      progress = progress_;
      _update(false);
    }

    void set_status(Status status_) {
      if (status == status_) return;
      status = status_;
      _update(true);
    }

    void set_message(std::string&& msg) {
      message = std::move(msg);
      _update(true);
    }

  private:
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
    void _update(bool force_render) {
      auto now = std::chrono::steady_clock::now();

      if (!visible) {
        auto tpassed = std::chrono::duration_cast<rtime_t>(now - time_start);
        double estimated_duration = tpassed.count() / std::max(progress, 0.1);
        double threshold_duration = min_duration * std::max(1.0, 2*progress);
        if (estimated_duration < threshold_duration) return;
        visible = true;
        force_render = true;
      }

      if (now >= time_next_update || force_render) {
        time_next_update = now + update_interval;
        if (progress_fn)
          _report_to_python();
        else
          _render();
      }
    }

    void _report_to_python() {
      PyObject* status_pyobj = status_strings[static_cast<int>(status)];
      progress_fn.call(py::otuple{ py::ofloat(progress),
                                   py::oobj(status_pyobj),
                                   py::ostring(message) });
    }

    void _render() {
      std::stringstream out;
      if (visible) out << '\r';
      _render_percentage(out);
      if (use_unicode) _render_progressbar_unicode(out);
      else             _render_progressbar_ascii(out);
      _render_message(out);
      _print_to_stdout(out);
    }

    void _render_percentage(std::stringstream& out) {
      int percentage = static_cast<int>(progress * 100 + 0.1);
      out << percentage << "% ";
      if (percentage < 10) out << ' ';
      if (percentage < 100) out << ' ';
    }

    void _render_progressbar_unicode(std::stringstream& out) {
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

    void _render_progressbar_ascii(std::stringstream& out) {
      int n_chars = static_cast<int>(progress * bar_width + 0.001);
      int i = 0;
      if (use_colors) out << "\x1B[2m";
      out << '[';
      for (; i < n_chars; ++i) out << '#';
      for (; i < bar_width; ++i) out << ' ';
      out << ']';
      if (use_colors) out << "\x1B[m";
    }

    void _render_message(std::stringstream& out) {
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

    void _print_to_stdout(std::stringstream& out) {
      auto pystr = py::ostring(out.str());
      outfile.invoke("write", pystr);
      outfile.invoke("flush");
    }
};



//------------------------------------------------------------------------------
// work
//------------------------------------------------------------------------------

work::work(double amount) {
  xassert(amount > 0);
  total_amount = amount;
  done_amount = 0;
  subtask_amount = 0;
  parent = current_work;
  if (parent) {
    multiplier = parent->multiplier * parent->subtask_amount / amount;
    parent_progress = parent->get_progress();
  } else {
    multiplier = 1 / amount;
    parent_progress = 0;
    current_work = this;
    pbar = std::unique_ptr<progress_bar>(new progress_bar);
  }
}


work::~work() {
  // if (parent) parent->done();  // ?
  if (current_work == this) current_work = nullptr;
}


void work::set_progress(double amount) {
  xassert(amount >= 0 && amount <= total_amount);
  done_amount = amount;
}


double work::get_progress() const {
  return parent_progress + done_amount * multiplier;
}


void work::update_progress_bar() {
  if (!pbar) return;
  pbar->set_progress(get_progress());
}

void work::set_status(Status s) {
  if (!pbar) return;
  pbar->set_status(s);
}

void work::set_message(std::string message) {
  if (!pbar) return;
  pbar->set_message(std::move(message));
}



work* current_progress() {
  return current_work;
}



}}  // namespace dt::progress
