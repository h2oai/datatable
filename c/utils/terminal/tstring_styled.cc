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
#include "utils/terminal/tstring.h"
#include "utils/terminal/terminal_style.h"
#include "encodings.h"
namespace dt {
using std::size_t;


/**
  * `tstring_styled` represents a string that has a certain
  * `TerminalStyle` applied. Multiple styles can be applied
  * simultaneously too:
  *
  *   tstring("TEST", style::bold|style::italic);
  *
  */
class tstring_styled : public tstring::impl {
  private:
    std::string   str_;
    size_t        size_;
    TerminalStyle style_;

  public:
    tstring_styled(const std::string&, TerminalStyle);
    tstring_styled(std::string&&, TerminalStyle);

    size_t size() const override;
    void write(TerminalStream&) const override;
    const std::string& str() override;
};




//------------------------------------------------------------------------------
// tstring_styled implementation
//------------------------------------------------------------------------------

tstring_styled::tstring_styled(const std::string& s, TerminalStyle style)
  : str_(s),
    size_(tstring::_compute_display_size(str_)),
    style_(style) {}


tstring_styled::tstring_styled(std::string&& s, TerminalStyle style)
  : str_(std::move(s)),
    size_(tstring::_compute_display_size(str_)),
    style_(style) {}


size_t tstring_styled::size() const {
  return size_;
}


void tstring_styled::write(TerminalStream& out) const {
  out << style_ << str_ << TerminalStyle::END;
}


const std::string& tstring_styled::str() {
  return str_;
}




//------------------------------------------------------------------------------
// tstring constructors
//------------------------------------------------------------------------------

tstring::tstring(const std::string& str, TerminalStyle style)
  : impl_{ std::make_shared<tstring_styled>(str, style) }
{}

tstring::tstring(std::string&& str, TerminalStyle style)
  : impl_{ std::make_shared<tstring_styled>(std::move(str), style) }
{}




}  // namespace dt
