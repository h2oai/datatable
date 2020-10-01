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

This option controls if the progress bar is cleared on success.

Parameters
----------
return: bool
    Current `clear_on_success` value. Initially, this option is set to `False`.

new_clear_on_success: bool
    New `clear_on_success` value. If `True`, the progress bar is cleared when
    job finished successfully. If `False`, the progress remains visible
    even when the job has already finished.

except: TypeError
    The exception is raised when the type of `new_clear_on_success` is not `bool`.

)";


bool clear_on_success = false;

static py::oobj get_clear_on_success() {
  return py::obool(clear_on_success);
}

static void set_clear_on_success(const py::Arg& arg) {
  clear_on_success = arg.to_bool_strict();
}

static void init_option_clear_on_success() {
  dt::register_option(
    "progress.clear_on_success",
    get_clear_on_success,
    set_clear_on_success,
    doc_options_progress_clear_on_success
  );
}


//------------------------------------------------------------------------------
// dt.options.progress.allow_interruption
//------------------------------------------------------------------------------

static const char* doc_options_progress_allow_interruption =
R"(

This option controls if the datatable tasks could be interrupted.


Parameters
----------
return: bool
    Current `allow_interruption` value. Initially, this option is set to `True`.

new_allow_interruption: bool
    New `allow_interruption` value. If `True`, datatable will be allowed
    to handle the `SIGINT` signal to interrupt long-running tasks.
    If `False`, it will not be possible to interrupt tasks with `SIGINT`.

except: TypeError
    The exception is raised when the type of `new_allow_interruption` is not `bool`.

)";


bool allow_interruption = true;


static py::oobj get_allow_interruption() {
  return py::obool(allow_interruption);
}

static void set_allow_interruption(const py::Arg& arg) {
  allow_interruption = arg.to_bool_strict();
}


static void init_option_allow_interruption() {
  dt::register_option(
    "progress.allow_interruption",
    get_allow_interruption,
    set_allow_interruption,
    doc_options_progress_allow_interruption
  );
}


//------------------------------------------------------------------------------
// dt.options.progress.enabled
//------------------------------------------------------------------------------

static const char* doc_options_progress_enabled =
R"(

This option controls if the progress reporting is enabled.

Parameters
----------
return: bool
    Current `enabled` value. Initially, this option is set to `True`
    if the `stdout` is connected to a terminal or a Jupyter Notebook,
    and `False` otherwise.

new_enabled: bool
    New `enabled` value. If `True`, the progress reporting
    functionality will be turned on. If `False`, it is turned off.

except: TypeError
    The exception is raised when the type of `new_enabled` is not `bool`.

)";


bool enabled = true;


static py::oobj get_enabled() {
  return py::obool(enabled);
}

static void set_enabled(const py::Arg& arg) {
  enabled = arg.to_bool_strict();
}


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
    get_enabled,
    set_enabled,
    doc_options_progress_enabled
  );
}



//------------------------------------------------------------------------------
// dt.options.progress.updates_per_second
//------------------------------------------------------------------------------

static const char* doc_options_progress_updates_per_second =
R"(

This option controls the progress bar update frequency.


Parameters
----------
return: float
    Current `updates_per_second` value. Initially, this option is set to `25.0`.

new_updates_per_second: float
    New `updates_per_second` value. This is the number of times per second
    the display of the progress bar should be updated.

except: TypeError
    The exception is raised when the type of `new_updates_per_second`
    is not numeric.

)";


double updates_per_second = 25.0;


static py::oobj get_updates_per_second() {
  return py::ofloat(updates_per_second);
}

static void set_updates_per_second(const py::Arg& arg) {
  double x = arg.to_double();
  py::Validator::check_finite(x, arg);
  py::Validator::check_positive(x, arg);
  updates_per_second = x;
}

static void init_option_updates_per_second() {
  dt::register_option(
    "progress.updates_per_second",
    get_updates_per_second,
    set_updates_per_second,
    doc_options_progress_updates_per_second
  );
}



//------------------------------------------------------------------------------
// dt.options.progress.min_duration
//------------------------------------------------------------------------------

static const char* doc_options_progress_min_duration =
R"(

This option controls the minimum duration of a task to show the progress bar.


Parameters
----------
return: float
    Current `min_duration` value. Initially, this option is set to `0.5`.

new_min_duration: float
    New `min_duration` value. The progress bar will not be shown
    if the duration of an operation is smaller than `new_min_duration`.
    If this value is non-zero, then the progress bar will only be shown
    for long-running operations, whose duration (estimated or actual)
    exceeds this threshold.

except: TypeError
    The exception is raised when the type of `new_min_duration`
    is not numeric.

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

This option controls the custom progress-reporting function.


Parameters
----------
return: function
    Current `callback` value. Initially, this option is set to `None`.

new_callback: function
    New `callback` value. If `None`, then the built-in progress-reporting
    function will be used. Otherwise, the `new_callback` specifies a function
    to be called at each progress event. The function should take a single
    parameter `p`, which is a namedtuple with the following fields:

    - `p.progress` is a float in the range `0.0 .. 1.0`;
    - `p.status` is a string, one of `'running'`, `'finished'`, `'error'` or
      `'cancelled'`;
    - `p.message` is a custom string describing the operation currently
      being performed.

except: TypeError
    The exception is raised when the type of `new_callback`
    is not `function`.

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
