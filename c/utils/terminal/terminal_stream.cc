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




TerminalStream::TerminalStream(bool use_colors) {
  use_colors_ = use_colors;
  stack_.push(style::reset);
  current_ = style::reset;
}

std::string TerminalStream::str() {
  xassert(stack_.size() == 1);
  _emit_pending_styles();
  return out_.str();
}


template <>
TerminalStream& TerminalStream::operator<<(const TerminalStyle& sty) {
  if (!use_colors_) return *this;
  if (sty == TerminalStyle::END) {
    xassert(stack_.size() >= 2);
    stack_.pop();
    return *this;
  }

  TerminalStyle newsty = stack_.top();
  if (style::has_style(sty)) {
    if (style::is_bold(sty))      style::set_bold(newsty);
    if (style::is_nobold(sty))    style::clear_bold(newsty);
    if (style::is_dim(sty))       style::set_dim(newsty);
    if (style::is_nodim(sty))     style::clear_dim(newsty);
    if (style::is_italic(sty))    style::set_italic(newsty);
    if (style::is_noitalic(sty))  style::clear_italic(newsty);
    if (style::is_underln(sty))   style::set_underln(newsty);
    if (style::is_nounderln(sty)) style::clear_underln(newsty);
  }
  if (style::is_nocolor(sty)) {
    style::clear_color(newsty);
  }
  if (style::has_color(sty)) {
    style::set_color(newsty, sty);
  }
  stack_.push(newsty);
  return *this;
}


template <>
TerminalStream& TerminalStream::operator<<(const tstring& s) {
  s.write_to(*this);
  return *this;
}




void TerminalStream::_emit_pending_styles() {
  if (!use_colors_) return;
  TerminalStyle newsty = stack_.top();
  TerminalStyle oldsty = current_;
  bool reset = (!style::is_bold(newsty)    && style::is_bold(oldsty)) ||
               (!style::is_dim(newsty)     && style::is_dim(oldsty)) ||
               (!style::is_italic(newsty)  && style::is_italic(oldsty)) ||
               (!style::is_underln(newsty) && style::is_underln(oldsty)) ||
               (!style::has_color(newsty)  && style::has_color(oldsty));
  bool add_bold    = style::is_bold(newsty)    && (reset || !style::is_bold(oldsty));
  bool add_dim     = style::is_dim(newsty)     && (reset || !style::is_dim(oldsty));
  bool add_italic  = style::is_italic(newsty)  && (reset || !style::is_italic(oldsty));
  bool add_underln = style::is_underln(newsty) && (reset || !style::is_underln(oldsty));
  bool add_fgcolor = style::has_color(newsty) &&
                     (reset || style::get_color(oldsty) != style::get_color(newsty));

  if (reset || add_bold || add_dim || add_italic || add_underln || add_fgcolor)
  {
    out_ << "\x1B[";
    if (reset)       { out_ << "0;"; current_ = style::reset; }
    if (add_bold)    { out_ << "1;"; style::set_bold(current_); }
    if (add_dim)     { out_ << "2;"; style::set_dim(current_); }
    if (add_italic)  { out_ << "3;"; style::set_italic(current_); }
    if (add_underln) { out_ << "4;"; style::set_underln(current_); }
    if (add_fgcolor) {
      out_ << style::get_color(newsty) << ';';
      style::set_color(current_, newsty);
    }
    out_.seekp(-1, out_.cur);  // overwrite the last ';'
    out_ << 'm';
  }
}



}  // namespace dt
