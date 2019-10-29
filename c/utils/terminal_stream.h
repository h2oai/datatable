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
namespace dt {
using std::size_t;



class TerminalStream {
  private:
    std::ostringstream out_;

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

    std::stack<TerminalStyle, std::vector<TerminalStyle>> stack_;
    TerminalStyle current_;

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

    TerminalStream();

    template <typename T>
    TerminalStream& operator<<(const T& value) {
      _emit_pending_styles();
      out_ << value;
    }

    std::string to_string() const;

  private:
    void _emit_pending_styles();
};


TerminalStream::TerminalStyle::TerminalStyle() {
  bold = 0;
  dim = 0;
  italic = 0;
  underln = 0;
  fgcolor = 0;
  bgcolor = 0;
}



TerminalStream::TerminalStream() {
  stack_.push(TerminalStyle());
}

std::string TerminalStream::to_string() const {
  return out_.str();
}


template <>
TerminalStream& TerminalStream::operator<<(const Modifier& style) {
  TerminalStyle newsty = stack_.top();  // copy
  switch (style) {
    case Modifier::END: {
      xassert(stack_.size() >= 2);
      stack_.pop();
      return *this;
    }
    case Modifier::BOLD:      newsty.bold = 1; break;
    case Modifier::NOBOLD:    newsty.bold = -1; break;
    case Modifier::DIM:       newsty.dim = 1; break;
    case Modifier::NODIM:     newsty.dim = -1; break;
    case Modifier::ITALIC:    newsty.italic = 1; break;
    case Modifier::NOITALIC:  newsty.italic = -1; break;
    case Modifier::UNDERLN:   newsty.underln = 1; break;
    case Modifier::NOUNDERLN: newsty.underln = -1; break;

    case Modifier::BLUE:      newsty.fgcolor = 34; break;
    case Modifier::BBLUE:     newsty.fgcolor = 94; break;
    case Modifier::CYAN:      newsty.fgcolor = 36; break;
    case Modifier::BCYAN:     newsty.fgcolor = 96; break;
    case Modifier::GREEN:     newsty.fgcolor = 32; break;
    case Modifier::BGREEN:    newsty.fgcolor = 92; break;
    case Modifier::GREY:      newsty.fgcolor = 90; break;
    case Modifier::MAGENTA:   newsty.fgcolor = 35; break;
    case Modifier::BMAGENTA:  newsty.fgcolor = 95; break;
    case Modifier::RED:       newsty.fgcolor = 31; break;
    case Modifier::BRED:      newsty.fgcolor = 91; break;
    case Modifier::WHITE:     newsty.fgcolor = 37; break;
    case Modifier::BWHITE:    newsty.fgcolor = 97; break;
    case Modifier::YELLOW:    newsty.fgcolor = 33; break;
    case Modifier::BYELLOW:   newsty.fgcolor = 93; break;
  }
  stack_.push(newsty);
  return *this;
}


void TerminalStream::_emit_pending_styles() {
  TerminalStyle newsty = stack_.top();
  TerminalStyle oldsty = current_;
  bool reset = (newsty.bold == -1    && oldsty.bold == 1) ||
               (newsty.dim == -1     && oldsty.dim == 1) ||
               (newsty.italic == -1  && oldsty.italic == 1) ||
               (newsty.underln == -1 && oldsty.underln == 1) ||
               (newsty.fgcolor == -1 && oldsty.fgcolor != 0) ||
               (newsty.bgcolor == -1 && oldsty.bgcolor != 0);
  bool add_bold    = (newsty.bold == 1)    && (reset || oldsty.bold == 0);
  bool add_dim     = (newsty.dim == 1)     && (reset || oldsty.dim == 0);
  bool add_italic  = (newsty.italic == 1)  && (reset || oldsty.italic == 0);
  bool add_underln = (newsty.underln == 1) && (reset || oldsty.underln == 0);
  bool add_fgcolor = (newsty.fgcolor > 0) &&
                     (reset || oldsty.fgcolor != newsty.fgcolor);
  bool add_bgcolor = (newsty.bgcolor > 0) &&
                     (reset || oldsty.bgcolor != newsty.bgcolor);

  if (reset || add_bold || add_dim || add_italic || add_underln ||
      add_fgcolor || add_bgcolor)
  {
    out_ << "\x1B[";
    if (reset)       { out_ << "0;"; current_ = TerminalStyle(); }
    if (add_bold)    { out_ << "1;"; current_.bold = 1; }
    if (add_dim)     { out_ << "2;"; current_.dim = 1; }
    if (add_italic)  { out_ << "3;"; current_.italic = 1; }
    if (add_underln) { out_ << "4;"; current_.underln = 1; }
    if (add_fgcolor) {
      out_ << newsty.fgcolor << ';';
      current_.fgcolor = newsty.fgcolor;
    }
    if (add_bgcolor) {
      out_ << newsty.bgcolor << ';';
      current_.bgcolor = newsty.bgcolor;
    }
    out_.seekp(-1, out_.cur);  // overwrite the last ';'
    out_ << 'm';
  }
}


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
