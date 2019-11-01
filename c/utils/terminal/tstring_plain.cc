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
namespace dt {

static size_t UNKNOWN = size_t(-1);


tstring_plain::tstring_plain()
  : str_(),
    size_(0) {}


tstring_plain::tstring_plain(const std::string& s)
  : str_(s),
    size_(UNKNOWN) {}


tstring_plain::tstring_plain(std::string&& s)
  : str_(std::move(s)),
    size_(UNKNOWN) {}



size_t tstring_plain::size() {
  if (size_ == UNKNOWN) size_ = _compute_display_size(str_);
  return size_;
}


void tstring_plain::write(TerminalStream& out) const {
  out << str_;
}


const std::string& tstring_plain::str() {
  return str_;
}


void tstring_plain::append(const std::string& str, tstring&) {
  str_ += str;
  size_ = UNKNOWN;
}


void tstring_plain::append(tstring&& str, tstring& parent) {
  auto plainstr = dynamic_cast<tstring_plain*>(str.impl_.get());
  if (plainstr) {
    str_ += plainstr->str_;
    size_ = UNKNOWN;
  } else {
    parent.convert_to_mixed();
    parent.impl_->append(std::move(str), parent);
  }
}




}  // namespace dt
