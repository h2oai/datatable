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
#include "csv/reader.h"           // GenericReader
#include "frame/py_frame.h"       // py::Frame
#include "python/_all.h"          // py::olist
#include "python/args.h"          // py::PKArgs
#include "python/string.h"        // py::ostring
#include "read/multisource.h"
namespace dt {
namespace read {

using SourcePtr = std::unique_ptr<Source>;
using SourceVec = std::vector<SourcePtr>;

static MultiSource _from_python(py::robj);
static MultiSource _from_any(py::robj, const GenericReader&);
static MultiSource _from_file(py::robj, const GenericReader&);
static MultiSource _from_text(const py::Arg&, const GenericReader&);
static MultiSource _from_cmd(py::robj, const GenericReader&);
static MultiSource _from_url(py::robj, const GenericReader&);


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

MultiSource::MultiSource(SourceVec&& srcs)
  : sources_(std::move(srcs)) {}


MultiSource::MultiSource(SourcePtr&& src) {
  sources_.push_back(std::move(src));
}


// Main MultiSource constructor
MultiSource MultiSource::from_args(const py::PKArgs& args,
                                   const GenericReader& rdr)
{
  const char* fnname = args.get_long_name();
  const py::Arg& src_any  = args[0];
  const py::Arg& src_file = args[1];
  const py::Arg& src_text = args[2];
  const py::Arg& src_cmd  = args[3];
  const py::Arg& src_url  = args[4];
  int total = src_any.is_defined() +
              src_file.is_defined() +
              src_text.is_defined() +
              src_cmd.is_defined() +
              src_url.is_defined();
  if (total > 1) {
    std::vector<const char*> extra_args;
    if (src_file.is_defined()) extra_args.push_back("file");
    if (src_text.is_defined()) extra_args.push_back("text");
    if (src_cmd.is_defined())  extra_args.push_back("cmd");
    if (src_url.is_defined())  extra_args.push_back("url");
    if (src_any.is_defined()) {
      throw TypeError()
          << "When an unnamed argument is passed to " << fnname
          << ", it is invalid to also provide the `"
          << extra_args[0] << "` parameter";
    } else {
      throw TypeError()
          << "Both parameters `" << extra_args[0] << "` and `" << extra_args[1]
          << "` cannot be passed to " << fnname << " simultaneously";
    }
  }

  if (src_any.is_defined())  return _from_any(src_any.to_oobj(), rdr);
  if (src_file.is_defined()) return _from_file(src_file.to_oobj(), rdr);
  if (src_text.is_defined()) return _from_text(src_text, rdr);
  if (src_cmd.is_defined())  return _from_cmd(src_cmd.to_oobj(), rdr);
  if (src_url.is_defined())  return _from_url(src_url.to_oobj(), rdr);

  throw TypeError()
      << "No input source for " << fnname
      << " was given. Please specify one of the parameters "
         "`file`, `text`, `url`, or `cmd`";
}


// temporary helper function
static MultiSource _from_python(py::robj pysource) {
  auto res_tuple = pysource.to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];

  SourceVec out;
  if (result.is_none()) {
    auto name = sources.to_otuple()[0].to_string();
    out.emplace_back(new Source_Python(name, sources));
  }
  else if (result.is_list_or_tuple()) {
    py::olist sources_list = result.to_pylist();
    for (size_t i = 0; i < sources_list.size(); ++i) {
      auto entry = sources_list[i].to_otuple();
      xassert(entry.size() == 2);
      auto isources = entry[0];
      auto iresult = entry[1];
      auto iname = isources.to_otuple()[0].to_string();
      if (iresult.is_none()) {
        out.emplace_back(new Source_Python(iname, isources));
      } else {
        out.emplace_back(new Source_Result(iname, iresult));
      }
    }
  }
  else if (result.is_dict()) {
    for (auto kv : result.to_rdict()) {
      out.emplace_back(new Source_Result(kv.first.to_string(), kv.second));
    }
  }
  else {
    out.emplace_back(new Source_Result("", result));
  }
  return MultiSource(std::move(out));
}


static MultiSource _from_any(py::robj src, const GenericReader& rdr) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_any");
  auto tempfiles = rdr.get_tempfiles();
  return _from_python(resolver.call({src, tempfiles}));
}


static MultiSource _from_file(py::robj src, const GenericReader& rdr) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_file");
  auto tempfiles = rdr.get_tempfiles();
  return _from_python(resolver.call({src, tempfiles}));
}


static MultiSource _from_text(const py::Arg& src, const GenericReader&) {
  if (!(src.is_string() || src.is_bytes())) {
    throw TypeError() << "Invalid parameter `text` in fread: expected "
                         "str or bytes, got " << src.typeobj();
  }
  return MultiSource(SourcePtr(new Source_Text(src.to_oobj())));
}


static MultiSource _from_cmd(py::robj src, const GenericReader&) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_cmd");
  return _from_python(resolver.call({src}));
}


static MultiSource _from_url(py::robj src, const GenericReader& rdr) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_url");
  auto tempfiles = rdr.get_tempfiles();
  return _from_python(resolver.call({src, tempfiles}));
}




//------------------------------------------------------------------------------
// Process sources, and return the results
//------------------------------------------------------------------------------

py::oobj MultiSource::read_all_fread_style(GenericReader& reader) {
  xassert(!sources_.empty());
  if (sources_.size() == 1) {
    const std::string& name = sources_[0]->name();
    py::oobj res = sources_[0]->read(reader);
    if (!name.empty()) res.to_pyframe()->set_source(name);
    return res;
  }
  else {
    py::odict result_dict;
    for (auto& src : sources_) {
      const std::string& iname = src->name();
      GenericReader ireader(reader);
      py::oobj res = src->read(ireader);
      if (!iname.empty()) res.to_pyframe()->set_source(iname);
      result_dict.set(py::ostring(iname), res);
    }
    return std::move(result_dict);
  }
}



}}  // namespace dt::read
