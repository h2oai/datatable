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
#ifndef dt_CSTRING_h
#define dt_CSTRING_h
#include "_dt.h"
namespace dt {



class CString {
  public:
    const char* ch;

  public:
    int64_t size;

  public:
    CString();   // default constructor creates an NA string
    CString(const std::string& str);
    CString(const char* ptr, int64_t sz);
    CString(const CString&);
    CString& operator=(const CString&);
    CString& operator=(const std::string& str);

    bool isna() const noexcept;
    bool operator==(const CString&) const noexcept;
    bool operator<(const CString&)  const noexcept;
    bool operator>(const CString&)  const noexcept;
    bool operator<=(const CString&) const noexcept;
    bool operator>=(const CString&) const noexcept;

  std::string to_string() const {
    return std::string(ch, static_cast<size_t>(size));
  }
};



}  // namespace dt
#endif
