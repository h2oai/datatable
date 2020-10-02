//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "frame/repr/repr_options.h"
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/arg.h"
#include "utils/terminal/terminal.h"
#include "options.h"
namespace dt {


static constexpr size_t NA_size_t = size_t(-1);
static constexpr int    MAX_int = 0x7FFFFFFF;

size_t display_max_nrows = 30;
size_t display_head_nrows = 15;
size_t display_tail_nrows = 5;
int    display_max_column_width = 100;
bool   display_interactive = false;
bool   display_use_colors = true;
bool   display_allow_unicode = true;

static const char* doc_options_display_use_colors =
R"(
This option controls whether or not to use colors when printing
datatable messages into the console. Turn this off if your terminal is unable to
display ANSI escape sequences, or if the colors make output not legible.

Parameters
----------
return: bool
    Current `use_colors` value. Initially, this option is set to `True`.

new_use_colors: bool
    New `use_colors` value.

except: TypeError
    The exception is raised when the type of `new_use_colors` is not `bool`.

)";

static const char* doc_options_display_allow_unicode =
R"(

This option controls whether or not unicode characters are
allowed in the datatable output.

Parameters
----------
return: bool
    Current `allow_unicode` value. Initially, this option is set to `True`.

new_allow_unicode: bool
    New `allow_unicode` value. If `True`, datatable will allow unicode
    characters (encoded as UTF-8) to be printed into the output.
    If `False`, then unicode characters will either be avoided, or
    hex-escaped as necessary.

except: TypeError
    The exception is raised when the type of `new_allow_unicode` is not `bool`.

)";

static const char* doc_options_display_interactive =
R"(

**Warning: This option is currently not working properly**
`[#2669] <https://github.com/h2oai/datatable/issues/2669>`_

This option controls the behavior of a Frame when it is viewed in a
text console. To enter the interactive mode manually, one can still
call the :meth:`Frame.view() <dt.Frame.view>` method.

Parameters
----------
return: bool
    Current `interactive` value. Initially, this option is set to `False`.

new_interactive: bool
    New `interactive` value. If `True`, frames will be shown in
    the interactove mode, allowing you to navigate the rows/columns
    with the keyboard. If `False`, frames will be shown in regular,
    non-interactive mode.

except: TypeError
    The exception is raised when the type of `new_interactive` is not `bool`.

)";

static const char* doc_options_display_max_nrows =
R"(

This option controls the threshold for the number of rows
in a frame to be truncated when printed to the console.

If a frame has more rows than `max_nrows`, it will be displayed
truncated: only its first
:attr:`head_nrows <datatable.options.display.head_nrows>`
and last
:attr:`tail_nrows <datatable.options.display.tail_nrows>`
rows will be printed. Otherwise, no truncation will occur.
It is recommended to have `head_nrows + tail_nrows <= max_nrows`.

Parameters
----------
return: int
    Current `max_nrows` value. Initially, this option is set to `30`.

new_max_nrows: int
    New `max_nrows` value. If this option is set to `None` or
    to a negative value, no frame truncation will occur when printed,
    which may cause the console to become unresponsive for frames
    with large number of rows.

except: TypeError
    The exception is raised when the type of `new_max_nrows` is not `int`.

)";

static const char* doc_options_display_tail_nrows =
R"(

This option controls the number of rows from the bottom of a frame
to be displayed when the frame's output is truncated due to the total number of
rows exceeding :attr:`max_nrows <datatable.options.display.max_nrows>`
value.

Parameters
----------
return: int
    Current `tail_nrows` value. Initially, this option is set to `5`.

new_tail_nrows: int
    New `tail_nrows` value, should be non-negative.

except: ValueError
    The exception is raised when the `new_tail_nrows` is negative.

except: TypeError
    The exception is raised when the type of `new_tail_nrows` is not `int`.

)";

static const char* doc_options_display_head_nrows =
R"(

This option controls the number of rows from the top of a frame
to be displayed when the frame's output is truncated due to the total number of
rows exceeding :attr:`display.max_nrows <datatable.options.display.max_nrows>`
value.

Parameters
----------
return: int
    Current `head_nrows` value. Initially, this option is set to `15`.

new_head_nrows: int
    New `head_nrows` value, should be non-negative.

except: ValueError
    The exception is raised when the `new_head_nrows` is negative.

except: TypeError
    The exception is raised when the type of `new_head_nrows` is not `int`.

)";


static const char* doc_options_display_max_column_width =
R"(

This option controls the threshold for the column's width
to be truncated. If a column's name or its values exceed
the `max_column_width`, the content of the column is truncated
to `max_column_width` characters when printed.

This option applies to both the rendering of a frame in a terminal,
and the rendering in a Jupyter notebook.

Parameters
----------
return: int
    Current `max_column_width` value. Initially, this option is set to `100`.

new_max_column_width: int
    New `max_column_width` value, cannot be less than `2`.
    If `new_max_column_width` equals to `None`, the column's content
    would never be truncated.

except: ValueError
    The exception is raised when the `new_max_column_width` is less than `2`.

except: TypeError
    The exception is raised when the type of `new_max_column_width` is not `int`.

)";


static py::oobj get_use_colors() {
  return py::obool(display_allow_unicode);
}

static void set_use_colors(const py::Arg& arg) {
  display_use_colors = arg.to_bool_strict();
  Terminal::standard_terminal().use_colors(display_use_colors);
}


static py::oobj get_allow_unicode() {
  return py::obool(display_allow_unicode);
}

static void set_allow_unicode(const py::Arg& arg) {
  display_allow_unicode = arg.to_bool_strict();
  Terminal::standard_terminal().use_unicode(display_allow_unicode);
}


static py::oobj get_interactive() {
  return py::obool(display_interactive);
}

static void set_interactive(const py::Arg& arg) {
  display_interactive = arg.to_bool_strict();
}


static py::oobj get_max_nrows() {
  return (display_max_nrows == NA_size_t)? py::None()
                                         : py::oint(display_max_nrows);
}

static void set_max_nrows(const py::Arg& arg) {
  if (arg.is_none()) {
    display_max_nrows = NA_size_t;
  } else {
    int64_t n = arg.to_int64_strict();
    display_max_nrows = (n < 0)? NA_size_t : static_cast<size_t>(n);
  }
}


static py::oobj get_head_nrows() {
  return py::oint(display_head_nrows);
}

static void set_head_nrows(const py::Arg& arg) {
  display_head_nrows = arg.to_size_t();
}


static py::oobj get_tail_nrows() {
  return py::oint(display_tail_nrows);
}

static void set_tail_nrows(const py::Arg& arg) {
  display_tail_nrows = arg.to_size_t();
}


static py::oobj get_max_column_width() {
  return (display_max_column_width == MAX_int)?
    py::None() : py::oint(display_max_column_width);
}

static void set_max_column_width(const py::Arg& arg) {
  const int max_column_width_limit = 2;
  if (arg.is_none()) {
    display_max_column_width = MAX_int;
  } else {
    int n = arg.to_int32_strict();

    if (n < max_column_width_limit) {
      throw ValueError() << "The smallest allowed value for `max_column_width`"
        << " is " << max_column_width_limit << ", got: " << n;
    }

    display_max_column_width = n;
  }
}


static void _init_options()
{
  register_option(
    "display.use_colors",
    get_use_colors,
    set_use_colors,
    doc_options_display_use_colors
  );

  register_option(
    "display.allow_unicode",
    get_allow_unicode,
    set_allow_unicode,
    doc_options_display_allow_unicode
  );

  register_option(
    "display.interactive",
    get_interactive,
    set_interactive,
    doc_options_display_interactive
  );

  register_option(
    "display.max_nrows",
    get_max_nrows,
    set_max_nrows,
    doc_options_display_max_nrows
  );

  register_option(
    "display.head_nrows",
    get_head_nrows,
    set_head_nrows,
    doc_options_display_head_nrows
  );

  register_option(
    "display.tail_nrows",
    get_tail_nrows,
    set_tail_nrows,
    doc_options_display_tail_nrows
  );

  register_option(
    "display.max_column_width",
    get_max_column_width,
    set_max_column_width,
    doc_options_display_max_column_width
  );
}




}  // namespace dt

void py::Frame::init_display_options() {
  dt::_init_options();
}
