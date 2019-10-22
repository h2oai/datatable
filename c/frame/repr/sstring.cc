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
#include "frame/repr/sstring.h"
#include "utils/assert.h"
#include "encodings.h"
namespace dt {
using std::size_t;



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

sstring::sstring()
  : str_(),
    size_(0) {}


sstring::sstring(const std::string& s)
  : str_(s),
    size_(_compute_string_size(str_)) {}


sstring::sstring(std::string&& s)
  : str_(std::move(s)),
    size_(_compute_string_size(str_)) {}


sstring::sstring(const std::string& s, size_t n)
  : str_(s),
    size_(n) {}


sstring::sstring(std::string&& s, size_t n)
  : str_(std::move(s)),
    size_(n) {}



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


size_t sstring::_compute_string_size(const std::string& str) {
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
