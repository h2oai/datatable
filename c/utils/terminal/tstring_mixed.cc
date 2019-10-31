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


/**
  * `tstring_mixed` is a string composed of multiple styled
  * fragments.
  *
  */
class tstring_mixed : public tstring::impl {
  private:
    std::vector<tstring> parts_;
    size_t size_;

  public:
    tstring_mixed();
    tstring_mixed(const tstring_mixed&) = default;
    tstring_mixed(tstring_mixed&&) noexcept = default;

    size_t size() const override;
    void write(TerminalStream&) const override;
    const std::string& str() override;

    void append(tstring&& str);
};




//------------------------------------------------------------------------------
// tstring_mixed implementation
//------------------------------------------------------------------------------

tstring_mixed::tstring_mixed() : size_(0) {}


size_t tstring_mixed::size() const {
  return size_;
}


void tstring_mixed::write(TerminalStream& out) const {
  for (const tstring& part : parts_) {
    out << part;
  }
}


const std::string& tstring_mixed::str() {
  return tstring::empty_;
}

void  tstring_mixed::append(tstring&& str) {
  size_ += str.size();
  parts_.emplace_back(std::move(str));
}




//------------------------------------------------------------------------------
// tstring operators
//------------------------------------------------------------------------------

tstring& tstring::operator<<(const tstring& str) {
  return *this << tstring(str);
}

tstring& tstring::operator<<(tstring&& str) {
  auto mix = dynamic_cast<tstring_mixed*>(impl_.get());
  if (mix == nullptr) {
    mix = new tstring_mixed();
    if (!empty()) {
      mix->append(std::move(*this));
    }
    impl_ = std::shared_ptr<impl>(mix);
  }
  mix->append(std::move(str));
  return *this;
}



}  // namespace dt
