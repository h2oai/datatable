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
  RESET     =       0,
  BOLD      = (1 << 0),
  NOBOLD    = (1 << 1),
  DIM       = (1 << 2),
  NODIM     = (1 << 3),
  ITALIC    = (1 << 4),
  NOITALIC  = (1 << 5),
  UNDERLN   = (1 << 6),
  NOUNDERLN = (1 << 7),
  ANYSTYLE  = 255,  // OR-ed enums BOLD..NOUNDERLN

  // colors
  BLUE      = (34 << 8),
  BBLUE     = (94 << 8),
  CYAN      = (36 << 8),
  BCYAN     = (96 << 8),
  GREEN     = (32 << 8),
  BGREEN    = (92 << 8),
  GREY      = (90 << 8),
  MAGENTA   = (25 << 8),
  BMAGENTA  = (95 << 8),
  RED       = (31 << 8),
  BRED      = (91 << 8),
  WHITE     = (37 << 8),
  BWHITE    = (97 << 8),
  YELLOW    = (33 << 8),
  BYELLOW   = (93 << 8),
  ANYCOLOR  = (127 << 8),  // OR-ed color enums
  NOCOLOR   = (1 << 15),   // disjoint with ANYCOLOR

  END       = size_t(-1)
};

inline TerminalStyle operator|(TerminalStyle a, TerminalStyle b) {
  return static_cast<TerminalStyle>(static_cast<size_t>(a) |
                                    static_cast<size_t>(b));
}




namespace style {
  static constexpr TerminalStyle reset     = TerminalStyle::RESET;
  static constexpr TerminalStyle bold      = TerminalStyle::BOLD;
  static constexpr TerminalStyle nobold    = TerminalStyle::NOBOLD;
  static constexpr TerminalStyle dim       = TerminalStyle::DIM;
  static constexpr TerminalStyle nodim     = TerminalStyle::NODIM;
  static constexpr TerminalStyle italic    = TerminalStyle::ITALIC;
  static constexpr TerminalStyle noitalic  = TerminalStyle::NOITALIC;
  static constexpr TerminalStyle underln   = TerminalStyle::UNDERLN;
  static constexpr TerminalStyle nounderln = TerminalStyle::NOUNDERLN;
  static constexpr TerminalStyle nocolor   = TerminalStyle::NOCOLOR;
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
  static constexpr TerminalStyle end       = TerminalStyle::END;


  inline constexpr bool has_style(TerminalStyle st)    { return (st & TerminalStyle::ANYSTYLE); }
  inline constexpr bool is_bold(TerminalStyle st)      { return (st & TerminalStyle::BOLD); }
  inline constexpr bool is_dim(TerminalStyle st)       { return (st & TerminalStyle::DIM); }
  inline constexpr bool is_italic(TerminalStyle st)    { return (st & TerminalStyle::ITALIC); }
  inline constexpr bool is_underln(TerminalStyle st)   { return (st & TerminalStyle::UNDERLN); }
  inline constexpr bool is_nobold(TerminalStyle st)    { return (st & TerminalStyle::NOBOLD); }
  inline constexpr bool is_nodim(TerminalStyle st)     { return (st & TerminalStyle::NODIM); }
  inline constexpr bool is_noitalic(TerminalStyle st)  { return (st & TerminalStyle::NOITALIC); }
  inline constexpr bool is_nounderln(TerminalStyle st) { return (st & TerminalStyle::NOUNDERLN); }

  inline void set_bold    (TerminalStyle& st) { st = st | style::bold; }
  inline void set_dim     (TerminalStyle& st) { st = st | style::dim; }
  inline void set_italic  (TerminalStyle& st) { st = st | style::italic; }
  inline void set_underln (TerminalStyle& st) { st = st | style::underln; }

  inline void clear_bold    (TerminalStyle& st) { st = static_cast<TerminalStyle>(st & ~style::bold); }
  inline void clear_dim     (TerminalStyle& st) { st = static_cast<TerminalStyle>(st & ~style::dim); }
  inline void clear_italic  (TerminalStyle& st) { st = static_cast<TerminalStyle>(st & ~style::italic); }
  inline void clear_underln (TerminalStyle& st) { st = static_cast<TerminalStyle>(st & ~style::underln); }

  inline constexpr bool has_color(TerminalStyle st)    { return (st & TerminalStyle::ANYCOLOR); }
  inline constexpr bool is_nocolor(TerminalStyle st)   { return (st & TerminalStyle::NOCOLOR); }

  inline constexpr size_t get_color(TerminalStyle st) {
    return (st & TerminalStyle::ANYCOLOR) >> 8;
  }
  inline void clear_color(TerminalStyle& st) {
    st = static_cast<TerminalStyle>(st & ~TerminalStyle::ANYCOLOR);
  }
  inline void set_color(TerminalStyle& st, TerminalStyle color) {
    st = static_cast<TerminalStyle>((st & ~TerminalStyle::ANYCOLOR) |
                                    (color & TerminalStyle::ANYCOLOR));
  }

}


}  // namespace dt
#endif
