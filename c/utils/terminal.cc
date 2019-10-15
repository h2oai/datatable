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
#include "utils/assert.h"
#include "utils/terminal.h"
namespace dt {



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

Terminal& Terminal::get_instance() {
  static Terminal term;
  return term;
}

Terminal::Terminal() {
  allow_unicode_ = true;
  enable_colors_ = true; // false;
  enable_ecma48_ = true; // false;
  enable_keyboard_ = false;
  is_jupyter_ = false;
  is_ipython_ = false;
  if (!enable_ecma48_) xassert(!enable_colors_);
}

Terminal::~Terminal() = default;



std::string Terminal::blue(const std::string& s) const {
  return enable_colors_? "\x1B[34m" + s + "\x1B[m" : s;
}

std::string Terminal::blueB(const std::string& s) const {
  return enable_colors_? "\x1B[94m" + s + "\x1B[m" : s;
}

std::string Terminal::bold(const std::string& s) const {
  return enable_colors_? "\x1B[1m" + s + "\x1B[m" : s;
}

std::string Terminal::cyan(const std::string& s) const {
  return enable_colors_? "\x1B[36m" + s + "\x1B[m" : s;
}

std::string Terminal::cyanB(const std::string& s) const {
  return enable_colors_? "\x1B[96m" + s + "\x1B[m" : s;
}

std::string Terminal::dim(const std::string& s) const {
  return enable_colors_? "\x1B[2m" + s + "\x1B[m" : s;
}

std::string Terminal::green(const std::string& s) const {
  return enable_colors_? "\x1B[32m" + s + "\x1B[m" : s;
}

std::string Terminal::greenB(const std::string& s) const {
  return enable_colors_? "\x1B[92m" + s + "\x1B[m" : s;
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
