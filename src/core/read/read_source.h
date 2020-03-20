//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#ifndef dt_READ_READ_SOURCE_h
#define dt_READ_READ_SOURCE_h
#include <memory>
#include <string>
#include "python/_all.h"
class GenericReader;
namespace dt {
namespace read {


class ReadSourceImpl;


class ReadSource
{
  using ImplPtr = std::unique_ptr<ReadSourceImpl>;
  private:
    std::string name_;
    ImplPtr impl_;

  public:
    ReadSource() = default;
    ReadSource(const ReadSource&) = delete;
    ReadSource(ReadSource&&) = default;
    ~ReadSource();

    // static ReadSource from_file(py::oobj src);
    // static ReadSource from_glob(py::oobj src);
    // static ReadSource from_text(py::oobj src);
    // static ReadSource from_url(py::oobj src);
    static ReadSource from_sources(py::oobj src, py::oobj name);
    static ReadSource from_result(py::oobj src, py::oobj name);

    const std::string& get_name() const { return name_; }
    py::oobj read(GenericReader& reader);

  private:
    ReadSource(ReadSourceImpl*);
    ReadSource(py::oobj name, ReadSourceImpl*);
};



std::vector<ReadSource> resolve_sources(const py::Arg& src_any,
                                        const py::Arg& src_file,
                                        const py::Arg& src_text,
                                        const py::Arg& src_cmd,
                                        const py::Arg& src_url,
                                        const GenericReader& gr);


}}  // namespace dt::read
#endif
