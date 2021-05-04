//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include "read2/read_director.h"
#include "read2/source.h"
#include "utils/assert.h"
namespace dt {
namespace read2 {


//------------------------------------------------------------------------------
// Source
//------------------------------------------------------------------------------

Source::Source(const std::string& name)
  : name_(name) {}

Source::~Source() {}

const std::string& Source::getName() const {
  return name_;
}

bool Source::keepReading() const {
  return false;
}




//------------------------------------------------------------------------------
// Source_Memory
//------------------------------------------------------------------------------

Source_Memory::Source_Memory(py::robj textsrc)
  : Source("<text>"),
    buffer_(Buffer::pybytes(textsrc))
{}


py::oobj Source_Memory::readWith(ReadDirector* director) {
  return director->readBuffer(buffer_);
}




//------------------------------------------------------------------------------
// Source_File
//------------------------------------------------------------------------------

Source_File::Source_File(std::string&& filename)
  : Source(filename),
    filename_(std::move(filename))
{}


py::oobj Source_File::readWith(ReadDirector* director) {
  auto buf = Buffer::mmap(filename_);
  return director->readBuffer(buf);
}




//------------------------------------------------------------------------------
// Source_Filelike
//------------------------------------------------------------------------------

Source_Filelike::Source_Filelike(py::robj file)
  : Source(file.safe_repr().to_string()),
    fileObject_(file) {}


py::oobj Source_Filelike::readWith(ReadDirector* director) {
  return director->readStream(BufferedStream::fromStream(
      std::unique_ptr<Stream>(new Stream_Filelike(fileObject_))
  ));
}




//------------------------------------------------------------------------------
// Source_Url
//------------------------------------------------------------------------------

Source_Url::Source_Url(const std::string& url)
  : Source(url),
    url_(url) {}


Source_Url::~Source_Url() {
  try {
    if (fileObject_) {
      fileObject_.invoke("close");
    }
  } catch (const std::exception&) {}
}


py::oobj Source_Url::readWith(ReadDirector* director) {
  auto urllib = py::oobj::import("urllib.request");
  fileObject_ = urllib.invoke("urlopen", {py::ostring(url_)});
  return director->readStream(BufferedStream::fromStream(
      std::unique_ptr<Stream>(new Stream_Filelike(fileObject_))
  ));
}




}}  // namespace dt::read2
