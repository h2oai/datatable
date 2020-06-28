//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "cstring.h"
#include "utils/assert.h"    // xassert
namespace dt {



//------------------------------------------------------------------------------
// CString constructors
//------------------------------------------------------------------------------

CString::CString()
  : ch(nullptr), size(0) {}

CString::CString(const CString& other)
  : ch(other.ch), size(other.size) {}

CString::CString(const char* ptr, int64_t sz)
  : ch(ptr), size(sz) {}

CString::CString(const std::string& str)
  : ch(str.data()), size(static_cast<int64_t>(str.size())) {}


CString& CString::operator=(const CString& other) {
  ch = other.ch;
  size = other.size;
  return *this;
}

CString& CString::operator=(const std::string& str) {
  ch = str.data();
  size = static_cast<int64_t>(str.size());
  return *this;
}



//------------------------------------------------------------------------------
// CString operators
//------------------------------------------------------------------------------

bool CString::operator==(const CString& other) const noexcept {
  if (ch == other.ch) {
    return (size == other.size) || (ch == nullptr);
  } else {
    return (size == other.size) && ch && other.ch &&
           (std::strncmp(ch, other.ch, static_cast<size_t>(size)) == 0);
  }
}


bool CString::operator<(const CString& other) const noexcept {
  if (ch == other.ch && size == other.size) {  // strings are equal
    return false;
  }
  size_t cmpsize = static_cast<size_t>(std::min(size, other.size));
  int comparison = std::strncmp(ch, other.ch, cmpsize);
  return comparison? (comparison < 0)
                   : (size < other.size);
}


bool CString::operator>(const CString& other) const noexcept {
  if (ch == other.ch && size == other.size) {  // strings are equal
    return false;
  }
  size_t cmpsize = static_cast<size_t>(std::min(size, other.size));
  int comparison = std::strncmp(ch, other.ch, cmpsize);
  return comparison? (comparison > 0)
                   : (size > other.size);
}


bool CString::operator<=(const CString& other) const noexcept {
  return !(*this > other);
}


bool CString::operator>=(const CString& other) const noexcept {
  return !(*this < other);
}




//------------------------------------------------------------------------------
// Miscellaneous
//------------------------------------------------------------------------------

bool CString::isna() const noexcept {
  return (ch == nullptr);
}


std::string CString::to_string() const {
  return std::string(ch, static_cast<size_t>(size));
}




}  // namespace dt
