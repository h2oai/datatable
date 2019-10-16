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
#include <string>
#ifndef dt_FRAME_REPR_SSTRING_h
#define dt_FRAME_REPR_SSTRING_h
namespace dt {
using std::size_t;


/**
  * This class represents a string whose display size in terminal
  * is different from the string's byte-size. The difference could
  * be due to:
  *
  *   - string containing unicode characters which are encoded as
  *     multi-byte UTF-8 sequences, yet are displayed as a single
  *     character on screen;
  *
  *   - string containing special terminal codes that affect the
  *     color of the text, yet are not visible in the output;
  *
  */
class sstring {
  private:
    std::string str_;
    size_t      size_;

  public:
    sstring();
    sstring(const sstring&) = default;
    sstring(sstring&&) noexcept = default;
    sstring& operator=(sstring&&) = default;
    sstring& operator=(const sstring&) = default;

    sstring(const std::string&);
    sstring(std::string&&);
    sstring(const std::string&, size_t n);
    sstring(std::string&&, size_t n);

    inline size_t size() const noexcept { return size_; }
    inline const std::string& str() const noexcept { return str_; }

  private:
    static size_t _compute_string_size(const std::string&);
};



}  // namespace dt
#endif
