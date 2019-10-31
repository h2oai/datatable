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
#ifndef dt_UTILS_TERMINAL_SSTRING_h
#define dt_UTILS_TERMINAL_SSTRING_h
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
    friend class sstring_plain;
    friend class sstring_stream;
    class impl {
      public:
        virtual ~impl();
        virtual size_t size() const = 0;
        virtual void write(TerminalStream&) const = 0;
        virtual const std::string& str() = 0;
    };
    std::shared_ptr<impl> impl_;

  public:
    sstring() = default;
    sstring(const sstring&) = default;
    sstring(sstring&&) noexcept = default;
    sstring& operator=(sstring&&) = default;
    sstring& operator=(const sstring&) = default;

    explicit sstring(const std::string&);
    explicit sstring(std::string&&);

    size_t size() const;
    const std::string& str() const;

  protected:
    static size_t _compute_display_size(const std::string&);
};



class sstring_plain : public sstring::impl {
  private:
    std::string str_;
    size_t      size_;

  public:
    sstring_plain();
    explicit sstring_plain(const std::string&);
    explicit sstring_plain(std::string&&);

    size_t size() const override;
    void write(TerminalStream&) const override;
    const std::string& str() override;
};



class sstring_stream : public sstring::impl {

};




}  // namespace dt
#endif
