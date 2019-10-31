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
#include "utils/terminal/tstring.h"
#include "utils/terminal/terminal_stream.h"
#include "utils/terminal/terminal_style.h"
namespace dt {



TerminalStream::TerminalStyle::TerminalStyle() {
  bold = 0;
  dim = 0;
  italic = 0;
  underln = 0;
  fgcolor = 0;
  bgcolor = 0;
}



TerminalStream::TerminalStream(bool use_colors) {
  use_colors_ = use_colors;
  stack_.push(TerminalStyle());
}

std::string TerminalStream::str() {
  xassert(stack_.size() == 1);
  _emit_pending_styles();
  return out_.str();
}


template <>
TerminalStream& TerminalStream::operator<<(const Modifier& style) {
  if (!use_colors_) return *this;
  if (style == Modifier::END) {
    xassert(stack_.size() >= 2);
    stack_.pop();
    return *this;
  }

  TerminalStyle newsty = stack_.top();  // copy
  if (style & 255) {  // font styles
    if (style & Modifier::BOLD)      newsty.bold = 1;
    if (style & Modifier::NOBOLD)    newsty.bold = 0;
    if (style & Modifier::DIM)       newsty.dim = 1;
    if (style & Modifier::NODIM)     newsty.dim = 0;
    if (style & Modifier::ITALIC)    newsty.italic = 1;
    if (style & Modifier::NOITALIC)  newsty.italic = 0;
    if (style & Modifier::UNDERLN)   newsty.underln = 1;
    if (style & Modifier::NOUNDERLN) newsty.underln = 0;
  }
  if (style & (255<<8)) {
    switch (style & (255<<8)) {
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
  }
  stack_.push(newsty);
  return *this;
}


template <>
TerminalStream& TerminalStream::operator<<(const tstring& s) {
  s.write_to(*this);
  return *this;
}


template <>
TerminalStream& TerminalStream::operator<<(const dt::TerminalStyle& style) {
  return *this << static_cast<Modifier>(style);
}



void TerminalStream::_emit_pending_styles() {
  if (!use_colors_) return;
  TerminalStyle newsty = stack_.top();
  TerminalStyle oldsty = current_;
  bool reset = (newsty.bold == 0    && oldsty.bold == 1) ||
               (newsty.dim == 0     && oldsty.dim == 1) ||
               (newsty.italic == 0  && oldsty.italic == 1) ||
               (newsty.underln == 0 && oldsty.underln == 1) ||
               (newsty.fgcolor == 0 && oldsty.fgcolor != 0) ||
               (newsty.bgcolor == 0 && oldsty.bgcolor != 0);
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
      out_ << static_cast<int>(newsty.fgcolor) << ';';
      current_.fgcolor = newsty.fgcolor;
    }
    if (add_bgcolor) {
      out_ << static_cast<int>(newsty.bgcolor) << ';';
      current_.bgcolor = newsty.bgcolor;
    }
    out_.seekp(-1, out_.cur);  // overwrite the last ';'
    out_ << 'm';
  }
}



}  // namespace dt
