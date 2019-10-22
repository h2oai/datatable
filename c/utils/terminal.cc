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
#include <iostream>
#include "frame/repr/repr_options.h"
#include "utils/assert.h"
#include "utils/terminal.h"
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
  display_allow_unicode = true;
  enable_colors_ = !is_plain;
  enable_ecma48_ = !is_plain;
  enable_keyboard_ = false;
  is_jupyter_ = false;
  is_ipython_ = false;
  if (!enable_ecma48_) xassert(!enable_colors_);
}

Terminal::~Terminal() = default;


void Terminal::initialize() {
  py::robj stdin = py::stdin();
  py::robj stdout = py::stdout();
  if (!stdout || !stdin || stdout.is_none() || stdin.is_none()) {
    enable_keyboard_ = false;
    enable_colors_ = false;
    enable_ecma48_ = false;
    display_allow_unicode = true;
  }
  else {
    // allow_unicode_ = false;
    enable_keyboard_ = true;
    enable_colors_ = true;
    enable_ecma48_ = true;
    // check  encoding?
    _check_ipython();
  }
  // Set options
  display_use_colors = enable_colors_;
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
  return display_allow_unicode;
}


void Terminal::use_colors(bool f) {
  enable_colors_ = f;
}

void Terminal::use_unicode(bool f) {
  display_allow_unicode = f;
}



//------------------------------------------------------------------------------
// Text formatting
//------------------------------------------------------------------------------

std::string Terminal::bold() const {
  return enable_colors_? "\x1B[1m" : "";
}

std::string Terminal::bold(const std::string& s) const {
  return enable_colors_? "\x1B[1m" + s + "\x1B[m" : s;
}


std::string Terminal::dim() const {
  return enable_colors_? "\x1B[2m" : "";
}

std::string Terminal::dim(const std::string& s) const {
  return enable_colors_? "\x1B[2m" + s + "\x1B[m" : s;
}


std::string Terminal::invert(const std::string& s) const {
  return enable_colors_? "\x1B[7m" + s + "\x1B[m" : s;
}


std::string Terminal::italic() const {
  return enable_colors_? "\x1B[3m" : "";
}

std::string Terminal::italic(const std::string& s) const {
  return enable_colors_? "\x1B[3m" + s + "\x1B[m" : s;
}


std::string Terminal::reset() const {
  return enable_colors_? "\x1B[m" : "";
}


std::string Terminal::underline(const std::string& s) const {
  return enable_colors_? "\x1B[4m" + s + "\x1B[m" : s;
}




//------------------------------------------------------------------------------
// Colors
//------------------------------------------------------------------------------

std::string Terminal::blue(const std::string& s) const {
  return enable_colors_? "\x1B[34m" + s + "\x1B[m" : s;
}

std::string Terminal::blueB(const std::string& s) const {
  return enable_colors_? "\x1B[94m" + s + "\x1B[m" : s;
}

std::string Terminal::cyan(const std::string& s) const {
  return enable_colors_? "\x1B[36m" + s + "\x1B[m" : s;
}

std::string Terminal::cyanB(const std::string& s) const {
  return enable_colors_? "\x1B[96m" + s + "\x1B[m" : s;
}

std::string Terminal::cyanD(const std::string& s) const {
  return enable_colors_? "\x1B[2;96m" + s + "\x1B[m" : s;
}

std::string Terminal::green(const std::string& s) const {
  return enable_colors_? "\x1B[32m" + s + "\x1B[m" : s;
}

std::string Terminal::greenB(const std::string& s) const {
  return enable_colors_? "\x1B[92m" + s + "\x1B[m" : s;
}

std::string Terminal::grey() const {
  return enable_colors_? "\x1B[90m" : "";
}

std::string Terminal::grey(const std::string& s) const {
  return enable_colors_? "\x1B[90m" + s + "\x1B[m" : s;
}

std::string Terminal::magenta(const std::string& s) const {
  return enable_colors_? "\x1B[35m" + s + "\x1B[m" : s;
}

std::string Terminal::magentaB(const std::string& s) const {
  return enable_colors_? "\x1B[95m" + s + "\x1B[m" : s;
}

std::string Terminal::red(const std::string& s) const {
  return enable_colors_? "\x1B[31m" + s + "\x1B[m" : s;
}

std::string Terminal::redB(const std::string& s) const {
  return enable_colors_? "\x1B[91m" + s + "\x1B[m" : s;
}

std::string Terminal::white(const std::string& s) const {
  return enable_colors_? "\x1B[37m" + s + "\x1B[m" : s;
}

std::string Terminal::whiteB(const std::string& s) const {
  return enable_colors_? "\x1B[97m" + s + "\x1B[m" : s;
}

std::string Terminal::yellow(const std::string& s) const {
  return enable_colors_? "\x1B[33m" + s + "\x1B[m" : s;
}

std::string Terminal::yellowB(const std::string& s) const {
  return enable_colors_? "\x1B[93m" + s + "\x1B[m" : s;
}



}  // namespace dt
