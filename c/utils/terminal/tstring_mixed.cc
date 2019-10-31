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
#include <vector>
#include "utils/assert.h"
#include "utils/terminal/tstring.h"
namespace dt {

static size_t UNKNOWN = size_t(-1);



tstring_mixed::tstring_mixed() : size_(0) {}


size_t tstring_mixed::size() {
  if (size_ == UNKNOWN) {
    size_ = 0;
    for (const auto& part : parts_) size_ += part.size();
  }
  return size_;
}


void tstring_mixed::write(TerminalStream& out) const {
  for (const tstring& part : parts_) {
    out << part;
  }
}


const std::string& tstring_mixed::str() {
  return tstring_impl::empty_;
}


void tstring_mixed::append(tstring&& str, tstring&) {
  size_ = UNKNOWN;
  parts_.emplace_back(std::move(str));
}


void tstring_mixed::append(const std::string& str, tstring&) {
  size_ = UNKNOWN;
  if (parts_.empty()) {
    parts_.emplace_back(str);
  }
  else {
    tstring& lastpart = parts_.back();
    if (dynamic_cast<const tstring_plain*>(lastpart.impl_.get())) {
      lastpart << str;
    } else {
      parts_.emplace_back(str);
    }
  }
}




}  // namespace dt
