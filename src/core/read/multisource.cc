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
#include "python/_all.h"          // py::olist
#include "python/args.h"          // py::PKArgs
#include "python/string.h"        // py::ostring
#include "read/multisource.h"
namespace dt {
namespace read {


//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

MultiSource::MultiSource(SourceVec&& srcs)
  : sources_(std::move(srcs)) {}


static MultiSource _from_python(py::robj pysource) {
  auto res_tuple = pysource.to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];

  std::vector<std::unique_ptr<Source>> out;
  if (result.is_none()) {
    out.emplace_back(new Source_Python("", sources));
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
  else {
    out.emplace_back(new Source_Result("", result));
  }
  return MultiSource(std::move(out));
}


MultiSource MultiSource::from_args(const py::PKArgs& args,
                                   const GenericReader& rdr)
{
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
  if (total != 1) {
    // Bad: throw an error; however, try to produce a message that reflects
    // what exactly is wrong with the parameters.
    const char* fnname = args.get_long_name();
    if (total == 0) {
      throw TypeError()
          << "No input source for " << fnname
          << " was given. Please specify one of the parameters "
             "`file`, `text`, `url`, or `cmd`";
    }
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

  auto resolve_source = py::oobj::import("datatable.utils.fread", "_resolve_source");
  auto tempfiles = rdr.get_tempfiles();
  auto source = py::otuple({src_any.to_oobj_or_none(),
                            src_file.to_oobj_or_none(),
                            src_text.to_oobj_or_none(),
                            src_cmd.to_oobj_or_none(),
                            src_url.to_oobj_or_none()});

  return _from_python(resolve_source.call({source, tempfiles}));
}




//------------------------------------------------------------------------------
// Process sources, and return the results
//------------------------------------------------------------------------------

py::oobj MultiSource::read_all_fread_style(GenericReader& reader) {
  xassert(!sources_.empty());
  if (sources_.size() == 1) {
    return sources_[0]->read(reader);
  }
  else {
    py::odict result_dict;
    for (auto& src : sources_) {
      GenericReader ireader(reader);
      result_dict.set(py::ostring(src->name()),
                      src->read(ireader));
    }
    return std::move(result_dict);
  }
}



}}  // namespace dt::read
