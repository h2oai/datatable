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
#ifndef dt_UTILS_TERMINAL_TSTRING_IMPL_h
#define dt_UTILS_TERMINAL_TSTRING_IMPL_h
namespace dt {
using std::size_t;



/**
  * Base class for a tstring implementation.
  *
  * The base impl can be used to represent an empty string only
  */
class tstring_impl {
  public:
    tstring_impl() = default;
    virtual ~tstring_impl();

    virtual size_t size();
    virtual void write(TerminalStream&) const;
    virtual const std::string& str();

    virtual void append(const std::string& str, tstring& parent);
    virtual void append(tstring&& str, tstring& parent);

  protected:
    static std::string empty_;
    static size_t _compute_display_size(const std::string&);
};



/**
  * Simplest kind of tstring that is a wrapper around a plain
  * std::string.
  */
class tstring_plain : public tstring_impl {
  private:
    std::string str_;
    size_t      size_;

  public:
    tstring_plain();
    explicit tstring_plain(const std::string&);
    explicit tstring_plain(std::string&&);

    size_t size() override;
    void write(TerminalStream&) const override;
    const std::string& str() override;

    void append(const std::string& str, tstring& parent) override;
    void append(tstring&& str, tstring& parent) override;
};



/**
  * `tstring_mixed` is a string composed of multiple styled
  * fragments.
  */
class tstring_mixed : public tstring_impl {
  private:
    std::vector<tstring> parts_;
    size_t size_;

  public:
    tstring_mixed();
    tstring_mixed(const tstring_mixed&) = default;
    tstring_mixed(tstring_mixed&&) noexcept = default;

    size_t size() override;
    void write(TerminalStream&) const override;
    const std::string& str() override;

    void append(const std::string& str, tstring& parent) override;
    void append(tstring&& str, tstring& parent) override;
};



/**
  * `tstring_styled` represents a string that has a certain
  * `TerminalStyle` applied. Multiple styles can be applied
  * simultaneously too:
  *
  *   tstring("TEST", style::bold|style::italic);
  *
  */
class tstring_styled : public tstring_impl {
  private:
    std::string   str_;
    size_t        size_;
    TerminalStyle style_;

  public:
    tstring_styled(const std::string&, TerminalStyle);
    tstring_styled(std::string&&, TerminalStyle);

    size_t size() override;
    void write(TerminalStream&) const override;
    const std::string& str() override;

    void append(const std::string& str, tstring& parent) override;
    void append(tstring&& str, tstring& parent) override;
};




}  // namespace dt
#endif
