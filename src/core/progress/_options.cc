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
#include "models/py_validator.h"
#include "progress/_options.h"
#include "python/_all.h"
#include "python/string.h"
#include "options.h"                // dt::register_option
namespace dt {
namespace progress {


//------------------------------------------------------------------------------
// dt.options.progress.clear_on_success
//------------------------------------------------------------------------------

static const char* doc_options_progress_clear_on_success =
R"(
If `True`, clear progress bar when job finished successfully.
)";


bool clear_on_success = false;

static void init_option_clear_on_success() {
  dt::register_option(
    "progress.clear_on_success",
    []{ return py::obool(clear_on_success); },
    [](const py::Arg& value){ clear_on_success = value.to_bool_strict(); },
    doc_options_progress_clear_on_success
  );
}


//------------------------------------------------------------------------------
// dt.options.progress.allow_interruption
//------------------------------------------------------------------------------

static const char* doc_options_progress_allow_interruption =
R"(
If `True`, allow datatable to handle the `SIGINT` signal to interrupt
long-running tasks.
)";


bool allow_interruption = true;

static void init_option_allow_interruption() {
  dt::register_option(
    "progress.allow_interruption",
    []{ return py::obool(allow_interruption); },
    [](const py::Arg& value){ allow_interruption = value.to_bool_strict(); },
    doc_options_progress_allow_interruption
  );
}


//------------------------------------------------------------------------------
// dt.options.progress.enabled
//------------------------------------------------------------------------------

static const char* doc_options_progress_enabled =
R"(
When `False`, progress reporting functionality will be turned off.
This option is `True` by default if the `stdout` is connected to a
terminal or a Jupyter Notebook, and False otherwise.
)";


bool enabled = true;

static bool stdout_is_a_terminal() {
  auto rstdout = py::rstdout();
  if (rstdout.is_none()) return false;
  py::oobj isatty = rstdout.get_attrx("isatty");
  if (!isatty) return false;
  py::oobj res = isatty.call();
  return res.is_bool()? res.to_bool_strict() : false;
}

static void init_option_enabled() {
  enabled = stdout_is_a_terminal();
  dt::register_option(
    "progress.enabled",
    []{ return py::obool(enabled); },
    [](const py::Arg& value){ enabled = value.to_bool_strict(); },
    doc_options_progress_enabled
  );
}



//------------------------------------------------------------------------------
// dt.options.progress.updates_per_second
//------------------------------------------------------------------------------

static const char* doc_options_progress_updates_per_second =
R"(
Number of times per second the display of the progress bar should be updated.
)";


double updates_per_second = 25.0;

static py::oobj get_ups() {
  return py::ofloat(updates_per_second);
}

static void set_ups(const py::Arg& value) {
  double x = value.to_double();
  py::Validator::check_finite(x, value);
  py::Validator::check_positive(x, value);
  updates_per_second = x;
}

static void init_option_updates_per_second() {
  dt::register_option(
    "progress.updates_per_second",
    get_ups,
    set_ups,
    doc_options_progress_updates_per_second
  );
}



//------------------------------------------------------------------------------
// dt.options.progress.min_duration
//------------------------------------------------------------------------------

static const char* doc_options_progress_min_duration =
R"(
Do not show progress bar if the duration of an operation is
smaller than this value. If this setting is non-zero, then
the progress bar will only be shown for long-running operations,
whose duration (estimated or actual) exceeds this threshold.
)";


double min_duration = 0.5;

static py::oobj get_min_duration() {
  return py::ofloat(min_duration);
}

static void set_min_duration(const py::Arg& value) {
  double x = value.to_double();
  py::Validator::check_not_negative(x, value);
  min_duration = x;
}

static void init_option_min_duration() {
  dt::register_option(
    "progress.min_duration",
    get_min_duration,
    set_min_duration,
    doc_options_progress_min_duration
  );
}



//------------------------------------------------------------------------------
// dt.options.progress.callback
//------------------------------------------------------------------------------

static const char* doc_options_progress_callback =
R"(
If `None`, then the built-in progress-reporting function will be used.
Otherwise, this value specifies a function to be called at each
progress event. The function takes a single parameter `p`, which is
a namedtuple with the following fields:

  - `p.progress` is a float in the range `0.0 .. 1.0`;
  - `p.status` is a string, one of 'running', 'finished', 'error' or
    'cancelled'; and
  - `p.message` is a custom string describing the operation currently
    being performed.
)";


// This cannot be `py::oobj`, because then the static variable will be destroyed
// on program's exit. However, at that point the Python runtime will have be
// already shut down, and trying to garbage-collect a python object at that
// moment leads to a segfault.
PyObject* progress_fn = nullptr;

static py::oobj get_callback() {
  return progress_fn? py::oobj(progress_fn) : py::None();
}

static void set_callback(const py::Arg& value) {
  py::oobj py_obj = value.to_oobj();
  Py_XSETREF(progress_fn,
             value.is_none()? nullptr : std::move(py_obj).release());
}

static void init_option_callback(){
  dt::register_option(
    "progress.callback",
    get_callback,
    set_callback,
    doc_options_progress_callback
  );
}



//------------------------------------------------------------------------------
// Init
//------------------------------------------------------------------------------

void init_options() {
  init_option_enabled();
  init_option_updates_per_second();
  init_option_min_duration();
  init_option_callback();
  init_option_clear_on_success();
  init_option_allow_interruption();
}



}} // namespace dt::progress
