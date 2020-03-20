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
#include "csv/reader.h"
#include "read/read_source.h"
#include "python/arg.h"
namespace dt {
namespace read {


class ReadSourceImpl {
  public:
    ReadSourceImpl() = default;
    virtual ~ReadSourceImpl() = default;

    virtual py::oobj read(GenericReader&) = 0;
};


class Text_ReadSource : public ReadSourceImpl {
  private:
    py::oobj src_;
    CString  text_;

  public:
    Text_ReadSource(py::oobj src)
      : src_(src), text_(src.to_cstring()) {}
};



// temporary
class Python_ReadSource : public ReadSourceImpl {
  private:
    py::oobj src_;

  public:
    Python_ReadSource(py::oobj src)
      : src_(src) {}

    py::oobj read(GenericReader& rdr) override {
      return rdr.read_all(src_);
    }
};


// temporary
class Result_ReadSource : public ReadSourceImpl {
  private:
    py::oobj result_;

  public:
    Result_ReadSource(py::oobj res)
      : result_(res) {}

    py::oobj read(GenericReader&) override {
      return result_;
    }
};



//------------------------------------------------------------------------------
// ReadSource
//------------------------------------------------------------------------------

ReadSource::~ReadSource() {}


ReadSource::ReadSource(ReadSourceImpl* impl)
  : impl_(std::move(impl)) {}

ReadSource::ReadSource(py::oobj name, ReadSourceImpl* impl)
  : impl_(std::move(impl))
{
  if (name && !name.is_none()) {
    name_ = name.to_string();
  }
}


// ReadSource ReadSource::from_text(py::oobj src) {
//   return ReadSource(new Text_ReadSource(src));
// }

ReadSource ReadSource::from_sources(py::oobj sources, py::oobj name) {
  return ReadSource(name, new Python_ReadSource(sources));
}

ReadSource ReadSource::from_result(py::oobj result, py::oobj name) {
  return ReadSource(name, new Result_ReadSource(result));
}

py::oobj ReadSource::read(GenericReader& reader) {
  return impl_->read(reader);
}


std::vector<ReadSource> resolve_sources(const py::Arg& src_any,
                                        const py::Arg& src_file,
                                        const py::Arg& src_text,
                                        const py::Arg& src_cmd,
                                        const py::Arg& src_url,
                                        const GenericReader& rdr)
{
  auto tempfiles = rdr.get_tempfiles();
  auto source = py::otuple({src_any.to_oobj_or_none(),
                            src_file.to_oobj_or_none(),
                            src_text.to_oobj_or_none(),
                            src_cmd.to_oobj_or_none(),
                            src_url.to_oobj_or_none()});

  auto resolve_source = py::oobj::import("datatable.utils.fread", "_resolve_source");
  auto res_tuple = resolve_source.call({source, tempfiles}).to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];

  std::vector<ReadSource> out;
  if (result.is_none()) {
    out.push_back(ReadSource::from_sources(sources, py::oobj()));
  }
  else if (result.is_list_or_tuple()) {
    py::olist result_list = result.to_pylist();
    for (size_t i = 0; i < result_list.size(); ++i) {
      auto entry = result_list[i].to_otuple();
      xassert(entry.size() == 2);
      auto isources = entry[0];
      auto iresult = entry[1];
      auto isrc = isources.to_otuple()[0];
      if (iresult.is_none()) {
        out.push_back(ReadSource::from_sources(isources, isrc));
      } else {
        out.push_back(ReadSource::from_result(iresult, isrc));
      }
    }
  }
  else {
    out.push_back(ReadSource::from_result(result, py::oobj()));
  }
  return out;
}



}}  // namespace dt::read
