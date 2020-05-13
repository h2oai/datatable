//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#include "csv/reader.h"     // GenericReader
#include "read/source.h"    // Source
#include "utils/macros.h"
#include "utils/misc.h"
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// Base Source class
//------------------------------------------------------------------------------

Source::Source(const std::string& name) : name_(name) {}

Source::~Source() {}


const std::string& Source::name() const {
  return name_;
}


std::unique_ptr<Source> Source::continuation() {
  return nullptr;
}




//------------------------------------------------------------------------------
// Source_Python  [temporary]
//------------------------------------------------------------------------------

Source_Python::Source_Python(const std::string& name, py::oobj src)
  : Source(name), src_(src) {}


py::oobj Source_Python::read(GenericReader& reader) {
  reader.source_name = &name_;

  auto pysrcs = src_.to_otuple();
  auto src_arg  = pysrcs[0];
  auto file_arg = pysrcs[1];
  int  fileno   = pysrcs[2].to_int32();
  auto text_arg = pysrcs[3];

  double t0 = wallclock();
  Buffer input_mbuf;
  CString text;
  const char* filename = nullptr;
  if (fileno > 0) {
    #if DT_OS_WINDOWS
      throw NotImplError() << "Reading from file-like objects, that involves "
        << "file descriptors, is not supported on Windows";
    #else
      const char* src = src_arg.to_cstring().ch;
      input_mbuf = Buffer::mmap(src, 0, fileno, false);
      size_t sz = input_mbuf.size();
      if (reader.verbose) {
        reader.d() << "Using file " << src << " opened at fd=" << fileno
                   <<"; size = " << sz;
      }
    #endif
  } else if ((text = text_arg.to_cstring())) {
    size_t size = static_cast<size_t>(text.size);
    input_mbuf = Buffer::external(text.ch, size + 1);

  } else if ((filename = file_arg.to_cstring().ch) != nullptr) {
    input_mbuf = Buffer::mmap(filename);
    size_t sz = input_mbuf.size();
    if (reader.verbose) {
      reader.d() << "File \"" << filename << "\" opened, size: " << sz;
    }

  } else {
    throw IOError() << "No input given to the GenericReader";
  }
  reader.t_open_input = wallclock() - t0;

  auto res = reader.read_buffer(input_mbuf, 0);
  reader.source_name = nullptr;
  return res;
}



//------------------------------------------------------------------------------
// Source_Result  [temporary]
//------------------------------------------------------------------------------

Source_Result::Source_Result(const std::string& name, py::oobj res)
  : Source(name), result_(res) {}


py::oobj Source_Result::read(GenericReader&) {
  return result_;
}



//------------------------------------------------------------------------------
// Source_Text
//------------------------------------------------------------------------------

Source_Text::Source_Text(py::robj textsrc)
  : Source("<text>"), src_(textsrc)
{
  xassert(src_.is_string() || src_.is_bytes());
}


py::oobj Source_Text::read(GenericReader& reader) {
  auto text = src_.to_cstring();
  auto buf = Buffer::external(text.ch, static_cast<size_t>(text.size) + 1);
  return reader.read_buffer(buf, 1);
}




}}  // namespace dt::read
