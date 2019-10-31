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
#include "encodings.h"
namespace dt {
using std::size_t;


tstring::impl::~impl() = default;


//------------------------------------------------------------------------------
// tstring_empty
//------------------------------------------------------------------------------

class tstring_empty : public tstring::impl {
  public:
    tstring_empty() = default;
    size_t size() const override;
    void write(TerminalStream&) const override;
    const std::string& str() override;
};

static std::shared_ptr<tstring_empty> EMPTY = std::make_shared<tstring_empty>();

size_t tstring_empty::size() const { return 0; }

void tstring_empty::write(TerminalStream&) const {}

const std::string& tstring_empty::str() { return tstring::empty_; }




//------------------------------------------------------------------------------
// tstring
//------------------------------------------------------------------------------
std::string tstring::empty_;

tstring::tstring()
  : impl_(EMPTY) {}


bool tstring::empty() const {
  return impl_ == EMPTY;
}


size_t tstring::size() const {
  return impl_->size();
}


const std::string& tstring::str() const {
  return impl_->str();
}


void tstring::write_to(TerminalStream& out) const {
  impl_->write(out);
}




//------------------------------------------------------------------------------
// Private helpers
//------------------------------------------------------------------------------

static inline bool isdigit(unsigned char c) {
  // Compiles to: (uint8_t)(c - '0') <= 9;
  return (c >= '0') && (c <= '9');
}

static inline bool isalpha(unsigned char c) {
  // Compiles to: (uint8_t)((c&~32) - 'A') <= 25;
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


size_t tstring::_compute_display_size(const std::string& str) {
  size_t n = str.size();
  size_t sz = 0;
  auto ch = reinterpret_cast<const unsigned char*>(str.data());
  auto end = ch + n;
  for (; ch < end; ) {
    auto c = *ch++;
    // ECMA-48 terminal control sequences
    if (c == 0x1B && ch < end && *ch == '[') {
      auto ch0 = ch;
      ch++;
      while (ch < end && isdigit(*ch)) ch++;
      if (ch < end && isalpha(*ch)) {
        ch++;
        continue;
      }
      ch = ch0;
    }

    if (c < 0x80) {
      sz++;
    }
    else {   // UTF-8 continuation sequences
      ch--;
      int ucp = read_codepoint_from_utf8(&ch);
      int w = mk_wcwidth(ucp);
      sz += static_cast<size_t>(w);
    }
  }
  return sz;
}




}  // namespace dt
