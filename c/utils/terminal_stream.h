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
#ifndef dt_UTILS_TERMINAL_STREAM_h
#define dt_UTILS_TERMINAL_STREAM_h
#include <stack>
#include <string>
#include <sstream>
#include <vector>
#include "utils/assert.h"
namespace dt {
using std::size_t;



class TerminalStream {
  private:
    struct TerminalStyle {
      int8_t bold;
      int8_t dim;
      int8_t italic;
      int8_t underln;
      int8_t fgcolor;
      int8_t bgcolor;
      int : 16;

      TerminalStyle();
      TerminalStyle(const TerminalStyle&) = default;
      TerminalStyle& operator=(const TerminalStyle&) = default;
    };

    std::ostringstream out_;
    std::stack<TerminalStyle, std::vector<TerminalStyle>> stack_;
    TerminalStyle current_;
    bool use_colors_;
    size_t : 56;

  public:
    TerminalStream(bool use_colors);

    template <typename T>
    TerminalStream& operator<<(const T& value) {
      _emit_pending_styles();
      out_ << value;
      return *this;
    }

    std::string str();

  public:
    enum Modifier : int8_t {
      END = 0,
      BOLD,
      NOBOLD,
      DIM,
      NODIM,
      ITALIC,
      NOITALIC,
      UNDERLN,
      NOUNDERLN,
      BLUE,
      BBLUE,
      CYAN,
      BCYAN,
      GREEN,
      BGREEN,
      GREY,
      MAGENTA,
      BMAGENTA,
      RED,
      BRED,
      WHITE,
      BWHITE,
      YELLOW,
      BYELLOW,
    };

  private:
    void _emit_pending_styles();
};



namespace style {
  static constexpr TerminalStream::Modifier end       = TerminalStream::Modifier::END;
  static constexpr TerminalStream::Modifier bold      = TerminalStream::Modifier::BOLD;
  static constexpr TerminalStream::Modifier nobold    = TerminalStream::Modifier::NOBOLD;
  static constexpr TerminalStream::Modifier dim       = TerminalStream::Modifier::DIM;
  static constexpr TerminalStream::Modifier nodim     = TerminalStream::Modifier::NODIM;
  static constexpr TerminalStream::Modifier italic    = TerminalStream::Modifier::ITALIC;
  static constexpr TerminalStream::Modifier noitalic  = TerminalStream::Modifier::NOITALIC;
  static constexpr TerminalStream::Modifier underln   = TerminalStream::Modifier::UNDERLN;
  static constexpr TerminalStream::Modifier nounderln = TerminalStream::Modifier::NOUNDERLN;
  static constexpr TerminalStream::Modifier blue      = TerminalStream::Modifier::BLUE;
  static constexpr TerminalStream::Modifier bblue     = TerminalStream::Modifier::BBLUE;
  static constexpr TerminalStream::Modifier cyan      = TerminalStream::Modifier::CYAN;
  static constexpr TerminalStream::Modifier bcyan     = TerminalStream::Modifier::BCYAN;
  static constexpr TerminalStream::Modifier green     = TerminalStream::Modifier::GREEN;
  static constexpr TerminalStream::Modifier bgreen    = TerminalStream::Modifier::BGREEN;
  static constexpr TerminalStream::Modifier grey      = TerminalStream::Modifier::GREY;
  static constexpr TerminalStream::Modifier magenta   = TerminalStream::Modifier::MAGENTA;
  static constexpr TerminalStream::Modifier bmagenta  = TerminalStream::Modifier::BMAGENTA;
  static constexpr TerminalStream::Modifier red       = TerminalStream::Modifier::RED;
  static constexpr TerminalStream::Modifier bred      = TerminalStream::Modifier::BRED;
  static constexpr TerminalStream::Modifier white     = TerminalStream::Modifier::WHITE;
  static constexpr TerminalStream::Modifier bwhite    = TerminalStream::Modifier::BWHITE;
  static constexpr TerminalStream::Modifier yellow    = TerminalStream::Modifier::YELLOW;
  static constexpr TerminalStream::Modifier byellow   = TerminalStream::Modifier::BYELLOW;
}



}  // namespace dt
#endif
