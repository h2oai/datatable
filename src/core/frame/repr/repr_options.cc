//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "documentation.h"
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
    dt::doc_options_display_use_colors
  );

  register_option(
    "display.allow_unicode",
    get_allow_unicode,
    set_allow_unicode,
    dt::doc_options_display_allow_unicode
  );

  register_option(
    "display.interactive",
    get_interactive,
    set_interactive,
    dt::doc_options_display_interactive
  );

  register_option(
    "display.max_nrows",
    get_max_nrows,
    set_max_nrows,
    dt::doc_options_display_max_nrows
  );

  register_option(
    "display.head_nrows",
    get_head_nrows,
    set_head_nrows,
    dt::doc_options_display_head_nrows
  );

  register_option(
    "display.tail_nrows",
    get_tail_nrows,
    set_tail_nrows,
    dt::doc_options_display_tail_nrows
  );

  register_option(
    "display.max_column_width",
    get_max_column_width,
    set_max_column_width,
    dt::doc_options_display_max_column_width
  );
}




}  // namespace dt

void py::Frame::init_display_options() {
  dt::_init_options();
}
