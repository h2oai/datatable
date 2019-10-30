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


static void _init_options()
{
  register_option(
    "display.use_colors",
    []{ return py::obool(display_use_colors); },
    [](const py::Arg& value) {
      display_use_colors = value.to_bool_strict();
      Terminal::standard_terminal().use_colors(display_use_colors);
      py::oobj::import("datatable.utils.terminal", "term")
        .invoke("use_colors", py::obool(display_use_colors));
    },
    "Whether to use colors when printing various messages into\n"
    "the console. Turn this off if your terminal is unable to\n"
    "display ANSI escape sequences, or if the colors make output\n"
    "not legible."
  );

  register_option(
    "display.allow_unicode",
    []{
      return py::obool(display_allow_unicode);
    },
    [](const py::Arg& value) {
      display_allow_unicode = value.to_bool_strict();
      Terminal::standard_terminal().use_unicode(display_allow_unicode);
      py::oobj::import("datatable.utils.terminal", "term")
        .invoke("set_allow_unicode", py::obool(display_allow_unicode));
    },
    "If True, datatable will allow unicode characters (encoded as\n"
    "UTF-8) to be printed into the output.\n"
    "If False, then unicode characters will either be avoided, or\n"
    "hex-escaped as necessary."
  );

  register_option(
    "display.interactive",
    []{ return py::obool(display_interactive); },
    [](const py::Arg& value) {
      display_interactive = value.to_bool_strict();
    },
    "This option controls the behavior of a Frame when it is viewed in a\n"
    "text console. When True, the Frame will be shown in the interactove\n"
    "mode, allowing you to navigate the rows/columns with keyboard.\n"
    "When False, the Frame will be shown in regular, non-interactive mode\n"
    "(you can still call DT.view() to enter the interactive mode manually.\n"
  );

  register_option(
    "display.max_nrows",
    []{
      return (display_max_nrows == NA_size_t)? py::None()
                                             : py::oint(display_max_nrows);
    },
    [](const py::Arg& value) {
      if (value.is_none()) {
        display_max_nrows = NA_size_t;
      } else {
        int64_t n = value.to_int64_strict();
        display_max_nrows = (n < 0)? NA_size_t : static_cast<size_t>(n);
      }
    },
    "A frame with more rows than this will be displayed truncated\n"
    "when the frame is printed to the console: only its first `head_nrows`\n"
    "and last `tail_nrows` rows will be printed. It is recommended to have\n"
    "`head_nrows + tail_nrows <= max_nrows`.\n"
    "Setting this option to None (or a negative value) will cause all\n"
    "rows in a frame to be printed, which may cause the console to become\n"
    "unresponsive.\n"
  );

  register_option(
    "display.head_nrows",
    []{
      return py::oint(display_head_nrows);
    },
    [](const py::Arg& value) {
      display_head_nrows = value.to_size_t();
    },
    "The number of rows from the top of a frame to be displayed when\n"
    "the frame's output is truncated due to the total number of frame's\n"
    "rows exceeding `max_nrows` value.\n"
  );

  register_option(
    "display.tail_nrows",
    []{
      return py::oint(display_tail_nrows);
    },
    [](const py::Arg& value) {
      display_tail_nrows = value.to_size_t();
    },
    "The number of rows from the bottom of a frame to be displayed when\n"
    "the frame's output is truncated due to the total number of frame's\n"
    "rows exceeding `max_nrows` value.\n"
  );

  register_option(
    "display.max_column_width",
    []{
      return (display_max_column_width == MAX_int)
              ? py::None()
              : py::oint(display_max_column_width);
    },
    [](const py::Arg& value) {
      if (value.is_none()) {
        display_max_column_width = MAX_int;
      } else {
        int n = value.to_int32_strict();
        display_max_column_width = (n < 0)? MAX_int : std::max(n, 2);
      }
    },
    "A column's name or values that exceed `max_column_width` in size\n"
    "will be truncated. This option applies both to rendering a frame\n"
    "in a terminal, and to rendering in a Jupyter notebook. The\n"
    "smallest allowed `max_column_width` is 2.\n"
    "Setting the value to `None` (or negative) indicates that the\n"
    "column's content should never be truncated.\n"
  );
}




}  // namespace dt

void py::Frame::init_display_options() {
  dt::_init_options();
}
