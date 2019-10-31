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
#include "utils/assert.h"
#include "utils/terminal/tstring.h"
#include "encodings.h"
namespace dt {
using std::size_t;


class tstring_plain : public tstring::impl {
  private:
    std::string str_;
    size_t      size_;

  public:
    tstring_plain();
    explicit tstring_plain(const std::string&);
    explicit tstring_plain(std::string&&);

    size_t size() const override;
    void write(TerminalStream&) const override;
    const std::string& str() override;
};




//------------------------------------------------------------------------------
// tstring_plain implementation
//------------------------------------------------------------------------------

tstring_plain::tstring_plain()
  : str_(),
    size_(0) {}


tstring_plain::tstring_plain(const std::string& s)
  : str_(s),
    size_(tstring::_compute_display_size(str_)) {}


tstring_plain::tstring_plain(std::string&& s)
  : str_(std::move(s)),
    size_(tstring::_compute_display_size(str_)) {}



size_t tstring_plain::size() const {
  return size_;
}


void tstring_plain::write(TerminalStream& out) const {
  out << str_;
}


const std::string& tstring_plain::str() {
  return str_;
}



tstring::tstring(const std::string& str)
  : impl_{ std::make_shared<tstring_plain>(str) }
{}

tstring::tstring(std::string&& str)
  : impl_{ std::make_shared<tstring_plain>(std::move(str)) }
{}


}  // namespace dt
