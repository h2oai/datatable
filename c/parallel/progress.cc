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
static double min_duration_s = 0.7;

void init_options() {
  dt::register_option(
    "progress.updates_per_second",
    []{ return py::ofloat(updates_per_second); },
    [](py::oobj value){ updates_per_second = value.to_double(); },
    "How often should the display of the progress bar be updated."
  );

  dt::register_option(
    "progress.min_duration",
    []{ return py::ofloat(min_duration_s); },
    [](py::oobj value){ min_duration_s = value.to_double(); },
    "Do not show progress bar if the duration of an operation is\n"
    "smaller than this value. If this setting is non-zero, then\n"
    "the progress bar will only be shown for long-running operations,\n"
    "whose duration (estimated or actual) exceeds this threshold.");
}




//------------------------------------------------------------------------------
// Internal progress bar
//------------------------------------------------------------------------------

class progress_bar {
  using ptime_t = std::chrono::time_point<std::chrono::steady_clock>;
  using dtime_t = std::chrono::steady_clock::duration;
  private:
    py::oobj outfile;
    ptime_t time_next_update;
    dtime_t update_interval;
    std::string message;
    double progress;  // [0.0 .. 1.0]
    int bar_width;
    bool visible;
    Status status;
    size_t : 16;

  public:
    progress_bar() {
      ptime_t time_start = std::chrono::steady_clock::now();
      std::chrono::duration<double> delta(1.0/updates_per_second);
      update_interval = std::chrono::duration_cast<dtime_t>(delta);
      time_next_update = time_start + update_interval;
      // PySys_GetObject returns borrowed ref!
      outfile = py::oobj(PySys_GetObject("stdout"));

      bar_width = 40;
      visible = false;
      status = Status::RUNNING;
    }

    void update_progress(double progress_, Status status_) {
      xassert(progress_ >= 0 && progress_ <= 1.0);
      bool status_updated = (status != status_);
      progress = progress_;
      status = status_;
      auto now = std::chrono::steady_clock::now();
      if (now >= time_next_update || (status_updated && visible)) {
        time_next_update = now + update_interval;
        _render();
        visible = true;
      }
    }

  private:
    void _render() {
      std::stringstream out;
      if (visible) out << '\r';
      _render_percentage(out);
      _render_progressbar_unicode(out);
      _render_message(out);
      _print_to_stdout(out);
    }

    void _render_percentage(std::stringstream& out) {
      int percentage = static_cast<int>(progress * 100 + 0.1);
      out << percentage << '%';
      if (percentage < 10) out << ' ';
      if (percentage < 100) out << ' ';
    }

    void _render_progressbar_unicode(std::stringstream& out) {
      double x = progress * bar_width;
      int n_full_chars = static_cast<int>(x + 0.001);
      int fractional_char = static_cast<int>((x - n_full_chars) * 8);
      int n_empty_chars = bar_width - n_full_chars - (fractional_char? 1 : 0);
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
    }

    void _render_message(std::stringstream& out) {
      out << ' ';
      switch (status) {
        case Status::RUNNING:   out << message; break;
        case Status::FINISHED:  out << "(done)" << '\n'; break;
        case Status::ERROR:     out << "(error)\n"; break;
        case Status::CANCELLED: out << "(cancelled)\n"; break;
      }
    }

    void _print_to_stdout(std::stringstream& out) {
      auto pystr = py::ostring(out.str());
      outfile.invoke("write", pystr);
      // outfile.invoke("flush");
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
  xassert(amount >= 0);
  done_amount = amount;
  xassert(done_amount <= total_amount * 1.00001);
}


double work::get_progress() const {
  return parent_progress + done_amount * multiplier;
}


void work::update_progress_bar() {
  if (!pbar) return;
  pbar->update_progress(get_progress(), Status::RUNNING);
}

void work::set_status(Status s) {
  if (pbar) pbar->update_progress(get_progress(), s);
}



work* current_progress() {
  return current_work;
}



}}  // namespace dt::progress
