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
#include "options.h"
namespace dt {


static constexpr size_t NA_size_t = size_t(-1);
size_t display_max_nrows = 50;
size_t display_head_nrows = 20;
size_t display_tail_nrows = 10;



static void _init_options() {
  dt::register_option(
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
}




}  // namespace dt

void py::Frame::init_display_options() {
  dt::_init_options();
}
