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
#include "utils/terminal/tstring_impl.h"
#include "encodings.h"
namespace dt {
using std::size_t;


static auto EMPTY_IMPL = std::make_shared<tstring_impl>();



//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

tstring::tstring()
  : impl_(EMPTY_IMPL) {}


tstring::tstring(const std::string& str)
  : impl_(std::make_shared<tstring_plain>(str)) {}


tstring::tstring(std::string&& str)
  : impl_(std::make_shared<tstring_plain>(std::move(str))) {}


tstring::tstring(const std::string& str, TerminalStyle style)
  : impl_(std::make_shared<tstring_styled>(str, style)) {}


tstring::tstring(std::string&& str, TerminalStyle style)
  : impl_(std::make_shared<tstring_styled>(std::move(str), style)) {}




//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

bool tstring::empty() const {
  return impl_ == EMPTY_IMPL;
}


size_t tstring::size() const {
  return impl_->size();
}


const std::string& tstring::str() const {
  return impl_->str();
}


void tstring::write_to(TerminalStream& out) const {
  impl_->write(out);
}



//------------------------------------------------------------------------------
// Appending strings
//------------------------------------------------------------------------------

tstring& tstring::operator<<(char c) {
  impl_->append(std::string(1, c), *this);
  return *this;
}


tstring& tstring::operator<<(const std::string& str) {
  impl_->append(str, *this);
  return *this;
}


tstring& tstring::operator<<(unsigned char c) {
  impl_->append(std::string(1, static_cast<char>(c)), *this);
  return *this;
}


tstring& tstring::operator<<(tstring&& str) {
  impl_->append(std::move(str), *this);
  return *this;
}


tstring& tstring::operator<<(const tstring& str) {
  impl_->append(tstring(str), *this);
  return *this;
}


void tstring::convert_to_mixed() {
  auto newimpl = new tstring_mixed;
  if (!empty()) {
    newimpl->append(std::move(*this), *this);
  }
  impl_ = std::shared_ptr<tstring_impl>(newimpl);
}



}  // namespace dt
