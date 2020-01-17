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
#include <csignal>
#include <iostream>
#include "frame/repr/repr_options.h"
#include "utils/assert.h"
#include "utils/macros.h"
#include "utils/terminal/terminal.h"

#if DT_OS_WINDOWS
  #include <windows.h>
#else
  #include <sys/ioctl.h>

  static void sigwinch_handler(int) {
    dt::Terminal::standard_terminal().forget_window_size();
  }
#endif

namespace dt {

//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

Terminal& Terminal::standard_terminal() {
  static Terminal term(false);
  return term;
}

Terminal& Terminal::plain_terminal() {
  static Terminal term(true);
  return term;
}

Terminal::Terminal(bool is_plain) {
  size_.width = is_plain? (1 << 20) : 0;
  size_.height = is_plain? 45 : 0;
  allow_unicode_ = true;
  enable_colors_ = !is_plain;
  enable_ecma48_ = !is_plain;
  enable_keyboard_ = false;
  is_jupyter_ = false;
  is_ipython_ = false;
  if (!enable_ecma48_) xassert(!enable_colors_);

  // Note: there is no simple way to catch the terminal re-size event
  // on Windows, because there is no `SIGWINCH` signal there.
  // For such a reason, on Windows we just re-check the terminal size
  // everytime the `get_width()` and `get_height()` are called.
  #if !DT_OS_WINDOWS
    if (!is_plain) {
      std::signal(SIGWINCH, sigwinch_handler);
    }
  #endif
}

Terminal::~Terminal() = default;


/**
  * This is called for 'standard' terminal only from "datatablemodule.cc",
  * once during module initialization.
  */
void Terminal::initialize() {
  py::robj rstdin = py::rstdin();
  py::robj rstdout = py::rstdout();
  if (!rstdout || !rstdin || rstdout.is_none() || rstdin.is_none()) {
    enable_keyboard_ = false;
    enable_colors_ = false;
    enable_ecma48_ = false;
    display_allow_unicode = true;
  }
  else {
    allow_unicode_ = false;
    try {
      std::string encoding = rstdout.get_attr("encoding").to_string();
      if (encoding == "UTF-8" || encoding == "UTF8" ||
          encoding == "utf-8" || encoding == "utf8") {
        allow_unicode_ = true;
      }
    } catch (...) {}
    enable_keyboard_ = true;
    enable_colors_ = true;
    enable_ecma48_ = true;
    _check_ipython();
  }
  // Set options
  display_use_colors = enable_colors_;
  display_allow_unicode = allow_unicode_;
}



/**
  * When running inside a Jupyter notebook, IPython and ipykernel will
  * already be preloaded (in sys.modules). We don't want to try to
  * import them, because it adds unnecessary startup delays.
  */
void Terminal::_check_ipython() {
  py::oobj ipython = py::get_module("IPython");
  if (ipython) {
    auto ipy = ipython.invoke("get_ipython");
    std::string ipy_type = ipy.typestr();
    if (ipy_type.find("ZMQInteractiveShell") != std::string::npos) {
      display_allow_unicode = true;
      is_jupyter_ = true;
    }
    if (ipy_type.find("TerminalInteractiveShell") != std::string::npos) {
      is_ipython_ = true;
    }
  }
}




//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

bool Terminal::is_jupyter() const noexcept {
  return is_jupyter_;
}

bool Terminal::is_ipython() const noexcept {
  return is_ipython_;
}

bool Terminal::colors_enabled() const noexcept {
  return enable_colors_;
}

bool Terminal::unicode_allowed() const noexcept {
  return allow_unicode_;
}


TerminalSize Terminal::get_size() {
  #if DT_OS_WINDOWS
    _detect_window_size();
  #else
    if (!size_.width || !size_.height) _detect_window_size();
  #endif
  return size_;
}


void Terminal::use_colors(bool f) {
  enable_colors_ = f;
}

void Terminal::forget_window_size() {
  size_.width = 0;
  size_.height = 0;
}

void Terminal::_detect_window_size() {
  #if DT_OS_WINDOWS
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    int ret = GetConsoleScreenBufferInfo(h, &csbi);
    bool size_not_detected = (ret == 0);
    size_.width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    size_.height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  #else
    struct winsize w;
    int ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    bool size_not_detected = (ret == -1);
    size_.width = w.ws_col;
    size_.height = w.ws_row;
  #endif

  if (size_not_detected || size_.width == 0) {
    size_.width = 120;
    size_.height = 45;
  }
}

void Terminal::use_unicode(bool f) {
  allow_unicode_ = f;
}



}  // namespace dt
