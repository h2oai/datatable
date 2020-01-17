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
#ifndef dt_UTILS_TERMINAL_TERMINAL_h
#define dt_UTILS_TERMINAL_TERMINAL_h
#include <memory>
#include <string>
namespace dt {
using std::size_t;

struct TerminalSize {
  int width;
  int height;
};


/**
  * Class that controls adorned output to a terminal. This class
  * supports setting basic text attributes (such as bold/italic),
  * and printing in different colors.
  *
  * In addition, this class has setting to fall back to regular
  * (non-colored) output if the terminal does not support it.
  *
  * This is a replacement for "terminal.py"
  */
class Terminal {
  using string = std::string;
  private:
    TerminalSize size_;
    bool allow_unicode_;
    bool enable_colors_;
    bool enable_ecma48_;
    bool enable_keyboard_;
    bool is_jupyter_;
    bool is_ipython_;
    int : 16;

  public:
    static Terminal& standard_terminal();
    static Terminal& plain_terminal();
    void initialize();

    bool is_jupyter() const noexcept;
    bool is_ipython() const noexcept;
    bool colors_enabled() const noexcept;
    bool unicode_allowed() const noexcept;
    TerminalSize get_size();

    void use_colors(bool f);
    void use_unicode(bool f);

    void forget_window_size();

  private:
    Terminal(bool is_plain);
    Terminal(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    ~Terminal();

    void _check_ipython();
    void _detect_window_size();
};



}  // namespace dt
#endif
