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



class CString
{
  private:
    const char* ptr_;
    size_t size_;

  public:
    explicit CString();   // default constructor creates an NA string
    explicit CString(const char* ptr, size_t size);
    explicit CString(const std::string&);
    CString(std::string&&) = delete;
    CString(const CString&) = delete;
    CString(CString&&) noexcept;
    CString& operator=(const CString&);
    CString& operator=(const std::string& str);
    CString& operator=(std::string&& str) = delete;

    bool operator==(const CString&) const noexcept;
    bool operator<(const CString&)  const noexcept;
    bool operator>(const CString&)  const noexcept;
    bool operator<=(const CString&) const noexcept;
    bool operator>=(const CString&) const noexcept;
    char operator[](size_t i) const;

    bool isna() const noexcept;
    size_t size() const noexcept;
    const char* data() const noexcept;
    const char* end() const noexcept;

    // Convert to a "regular" C++ string. If this CString is NA,
    // then an empty string will be returned.
    std::string to_string() const;
};



}  // namespace dt
#endif
