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
#include "buffer.h"
namespace dt {



/**
  * This class represents a string that can be easily passed around
  * without copying the data. The downside is that the pointer it
  * returns is not owned by this class, so there is always a chance
  * to have a dangling reference.
  *
  * As such, whenever a function returns a CString, it must ensure
  * that the CString is pointing to a reasonably stable underlying
  * string object. Conversely, if a user receives a CString from a
  * function, it must use it right away, and do not attempt to store
  * for a prolonged period of time.
  *
  * Another possibility when a function needs to create and return
  * a new string, is to use the built-in `buffer_` variable. This
  * variable is not normally accessible from the outside, but it may
  * serve as a character buffer that will remain alive as long as the
  * CString object is kept alive.
  */
class CString
{
  private:
    const char* ptr_;
    size_t size_;
    Buffer buffer_;

  public:
    explicit CString();   // default constructor creates an NA string
    explicit CString(const char* ptr, size_t size);
    explicit CString(const std::string&);
    CString(std::string&&) = delete;
    CString(const CString&);
    CString(CString&&) noexcept;
    CString& operator=(CString&&);
    CString& operator=(const std::string& str);
    CString& operator=(const CString&) = delete;
    CString& operator=(std::string&& str) = delete;
    static CString from_null_terminated_string(const char* cstr);

    bool operator==(const CString&) const noexcept;
    bool operator<(const CString&)  const noexcept;
    bool operator>(const CString&)  const noexcept;
    bool operator<=(const CString&) const noexcept;
    bool operator>=(const CString&) const noexcept;
    char operator[](size_t i) const;

    // Replace CString's contents with new ptr/size
    void set(const char* data, size_t size);
    void set_na();
    void set_data(const char* data);
    void set_size(size_t size);

    bool isna() const noexcept;
    size_t size() const noexcept;
    const char* data() const noexcept;
    const char* end() const noexcept;

    // Convert to a "regular" C++ string. If this CString is NA,
    // then an empty string will be returned.
    std::string to_string() const;

    // Internal buffer is prepared to write `size` bytes of data,
    // the pointers `ptr_`+`size_` are set to match the internal
    // buffer, so that if the user writes exactly the requested
    // amount of data, the CString object will be ready.
    // If the requested `size` is 0, the pointer `ptr_` will be set to
    // some non-NULL value, so that the CString's value is equivalent
    // to an empty string (not NA).
    char* prepare_buffer(size_t size);
};



}  // namespace dt
#endif
