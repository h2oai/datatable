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
#ifndef dt_UTILS_TERMINAL_TERMINAL_STYLE_h
#define dt_UTILS_TERMINAL_TERMINAL_STYLE_h
#include <cstdint>
namespace dt {
using std::size_t;



enum TerminalStyle : size_t {
  END       =       0,
  BOLD      = (1 << 0),
  NOBOLD    = (1 << 1),
  DIM       = (1 << 2),
  NODIM     = (1 << 3),
  ITALIC    = (1 << 4),
  NOITALIC  = (1 << 5),
  UNDERLN   = (1 << 6),
  NOUNDERLN = (1 << 7),
  // colors
  BLUE      = (1 << 8),
  BBLUE     = (2 << 8),
  CYAN      = (3 << 8),
  BCYAN     = (4 << 8),
  GREEN     = (5 << 8),
  BGREEN    = (6 << 8),
  GREY      = (7 << 8),
  MAGENTA   = (8 << 8),
  BMAGENTA  = (9 << 8),
  RED       = (10 << 8),
  BRED      = (11 << 8),
  WHITE     = (12 << 8),
  BWHITE    = (13 << 8),
  YELLOW    = (14 << 8),
  BYELLOW   = (15 << 8),
};

inline TerminalStyle operator|(TerminalStyle a, TerminalStyle b) {
  return static_cast<TerminalStyle>(static_cast<size_t>(a) |
                                    static_cast<size_t>(b));
}



namespace style {
  static constexpr TerminalStyle end       = TerminalStyle::END;
  static constexpr TerminalStyle bold      = TerminalStyle::BOLD;
  static constexpr TerminalStyle nobold    = TerminalStyle::NOBOLD;
  static constexpr TerminalStyle dim       = TerminalStyle::DIM;
  static constexpr TerminalStyle nodim     = TerminalStyle::NODIM;
  static constexpr TerminalStyle italic    = TerminalStyle::ITALIC;
  static constexpr TerminalStyle noitalic  = TerminalStyle::NOITALIC;
  static constexpr TerminalStyle underln   = TerminalStyle::UNDERLN;
  static constexpr TerminalStyle nounderln = TerminalStyle::NOUNDERLN;
  static constexpr TerminalStyle blue      = TerminalStyle::BLUE;
  static constexpr TerminalStyle bblue     = TerminalStyle::BBLUE;
  static constexpr TerminalStyle cyan      = TerminalStyle::CYAN;
  static constexpr TerminalStyle bcyan     = TerminalStyle::BCYAN;
  static constexpr TerminalStyle green     = TerminalStyle::GREEN;
  static constexpr TerminalStyle bgreen    = TerminalStyle::BGREEN;
  static constexpr TerminalStyle grey      = TerminalStyle::GREY;
  static constexpr TerminalStyle magenta   = TerminalStyle::MAGENTA;
  static constexpr TerminalStyle bmagenta  = TerminalStyle::BMAGENTA;
  static constexpr TerminalStyle red       = TerminalStyle::RED;
  static constexpr TerminalStyle bred      = TerminalStyle::BRED;
  static constexpr TerminalStyle white     = TerminalStyle::WHITE;
  static constexpr TerminalStyle bwhite    = TerminalStyle::BWHITE;
  static constexpr TerminalStyle yellow    = TerminalStyle::YELLOW;
  static constexpr TerminalStyle byellow   = TerminalStyle::BYELLOW;
}


}  // namespace dt
#endif
