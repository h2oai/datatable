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
#include <memory>
#include <string>
#include "utils/terminal/terminal_stream.h"
#include "utils/terminal/terminal_style.h"
#include "utils/terminal/tstring_impl.h"
#ifndef dt_UTILS_TERMINAL_TSTRING_h
#define dt_UTILS_TERMINAL_TSTRING_h
namespace dt {
using std::size_t;


/**
  * `tstring` is a string that we want to later display in a terminal.
  * This class is different from a regular string in two respects:
  *
  *   - The `size()` method returns the display size of the string.
  *     This will not be equal to string's byte-size if the string
  *     contains Unicode characters, some of which may even be
  *     double-width or zero-width;
  *
  *   - The string may be styled, i.e. it may have a font effect
  *     (such as bold/italic) or a color applied. We even allow a
  *     `tstring` to contain multiple different-styled fragments.
  *
  * The class uses standard Pimpl idiom, allowing different
  * implementations depending on the complexity of the underlying
  * string. The following API is exposed:
  *
  *   - size() : returns the display width of this string, taking
  *       into account characters that may have double or zero display
  *       width;
  *
  *   - write_to(out) : write the content of this string into the
  *       provided output stream. Alternatively you can also write
  *       `out << tstr`;
  *
  *   - str() : access the underlying raw string. This function is
  *       only available for "simple" sstrings.
  *
  */
class tstring
{
  private:
    std::shared_ptr<tstring_impl> impl_;

  public:
    tstring();
    tstring(const tstring&) = default;
    tstring(tstring&&) noexcept = default;
    tstring& operator=(tstring&&) = default;
    tstring& operator=(const tstring&) = default;

    explicit tstring(const std::string&);
    explicit tstring(std::string&&);
    tstring(const std::string&, TerminalStyle);
    tstring(std::string&&, TerminalStyle);

    size_t size() const;
    void write_to(TerminalStream&) const;
    const std::string& str() const;
    bool empty() const;

    tstring& operator<<(tstring&&);
    tstring& operator<<(const tstring&);
    tstring& operator<<(const std::string&);
    tstring& operator<<(char c);
    tstring& operator<<(unsigned char c);

  private:
    void convert_to_mixed();

    friend class tstring_mixed;
    friend class tstring_plain;
    friend class tstring_styled;
};




}  // namespace dt
#endif
