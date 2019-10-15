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

static inline bool isdigit(char c) {
  // Compiles to: (uint8_t)(c - '0') <= 9;
  return (c >= '0') && (c <= '9');
}

static inline bool isalpha(char c) {
  // Compiles to: (uint8_t)((c&~32) - 'A') <= 25;
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}


size_t sstring::_compute_string_size(const std::string& str) {
  size_t n = str.size();
  size_t sz = 0;
  for (size_t i = 0; i < n; ++i) {
    auto c = static_cast<unsigned char>(str[i]);
    // Ecma-48 terminal control sequences
    if (c == 0x1B && i < n-2 && str[i+1] == '[') {
      size_t j = i + 2;
      while (j < n && isdigit(str[j])) j++;
      if (j < n && isalpha(str[j])) {
        i = j;  // i will be incremented by the loop too
        continue;
      }
    }
    // UTF-8 continuation sequences
    if (c >= 0xC0) {
      if ((c & 0xE0) == 0xC0)      i += 1;
      else if ((c & 0xF0) == 0xE0) i += 2;
      else if ((c & 0xF8) == 0xF0) i += 3;
    }
    sz++;
  }
  return sz;
}




}  // namespace dt
