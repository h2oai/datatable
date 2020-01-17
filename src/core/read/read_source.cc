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
#include "read/read_source.h"
namespace dt {
namespace read {


class ReadSourceImpl {
  public:
    ReadSourceImpl() = default;
    virtual ~ReadSourceImpl() = default;
};


class Text_ReadSource : public ReadSourceImpl {
  private:
    py::oobj src_;
    CString  text_;

  public:
    Text_ReadSource(py::oobj src)
      : src_(src), text_(src.to_cstring()) {}
};




//------------------------------------------------------------------------------
// ReadSource
//------------------------------------------------------------------------------

ReadSource::ReadSource() = default;
ReadSource::ReadSource(ReadSource&&) = default;
ReadSource::~ReadSource() = default;

ReadSource::ReadSource(ReadSourceImpl* impl)
  : impl_(std::move(impl)) {}


ReadSource ReadSource::from_text(py::oobj src) {
  return ReadSource(new Text_ReadSource(src));
}


py::oobj ReadSource::read_one() {
  return py::None();
}

py::oobj ReadSource::read_all() {
  return py::None();
}



}}  // namespace dt::read
