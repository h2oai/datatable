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
#include "csv/reader.h"                // GenericReader
#include "frame/py_frame.h"            // py::Frame
#include "progress/progress_manager.h" // dt::progress::manager
#include "python/_all.h"               // py::olist
#include "python/xargs.h"              // py::XArgs
#include "python/string.h"             // py::ostring
#include "read/multisource.h"
#include "utils/macros.h"
namespace dt {
namespace read {

using SourcePtr = std::unique_ptr<Source>;
using SourceVec = std::vector<SourcePtr>;

static SourceVec _from_python(py::robj);
static SourceVec _from_any(py::robj, const GenericReader&);
static SourceVec _from_file(py::robj, const GenericReader&);
static SourceVec _from_text(const py::Arg&, const GenericReader&);
static SourceVec _from_cmd(py::robj, const GenericReader&);
static SourceVec _from_url(py::robj, const GenericReader&);

static SourceVec single_source(Source* src) {
  SourceVec res;
  res.push_back(SourcePtr(src));
  return res;
}

#define D() if (rdr.verbose) rdr.logger_.info()




//------------------------------------------------------------------------------
// Constructors
//------------------------------------------------------------------------------

MultiSource::MultiSource(const py::XArgs& args, GenericReader&& rdr)
  : reader_(std::move(rdr)),
    sources_(),
    iteration_index_(0)
{
  const auto& fnname = args.proper_name();
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
    if (total == 0) {
      throw TypeError()
          << "No input source for " << fnname
          << "() was given. Please specify one of the parameters "
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
          << "(), it is invalid to also provide the `"
          << extra_args[0] << "` parameter";
    } else {
      throw TypeError()
          << "Both parameters `" << extra_args[0] << "` and `" << extra_args[1]
          << "` cannot be passed to " << fnname << "() simultaneously";
    }
  }

  if (src_any.is_defined())  sources_ = _from_any(src_any.to_oobj(), reader_);
  if (src_file.is_defined()) sources_ = _from_file(src_file.to_oobj(), reader_);
  if (src_text.is_defined()) sources_ = _from_text(src_text, reader_);
  if (src_cmd.is_defined())  sources_ = _from_cmd(src_cmd.to_oobj(), reader_);
  if (src_url.is_defined())  sources_ = _from_url(src_url.to_oobj(), reader_);
}


// temporary helper function
static SourceVec _from_python(py::robj pysource) {
  auto res_tuple = pysource.to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];
  auto name = sources.to_otuple()[0].to_string();

  SourceVec out;
  if (result.is_none()) {
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
    out.emplace_back(new Source_Result(name, result));
  }
  return out;
}



static SourceVec _from_file(py::robj src, const GenericReader& rdr) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_file");
  auto tempfiles = rdr.get_tempfiles();
  return _from_python(resolver.call({src, tempfiles}));
}


static SourceVec _from_text(const py::Arg& src, const GenericReader&) {
  if (!(src.is_string() || src.is_bytes())) {
    throw TypeError() << "Invalid parameter `text` in fread: expected "
                         "str or bytes, got " << src.typeobj();
  }
  SourceVec out;
  out.emplace_back(new Source_Text(src.to_oobj()));
  return out;
}


static SourceVec _from_cmd(py::robj src, const GenericReader&) {
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_cmd");
  return _from_python(resolver.call({src}));
}


static SourceVec _from_url(py::robj src, const GenericReader&) {
  return single_source(new Source_Url(src.to_string()));
}




//------------------------------------------------------------------------------
// Resolve "any_source" parameter to fread
//------------------------------------------------------------------------------

// Return true if `text` has any characters from C0 range.
static bool _has_control_characters(const CString& text, char* evidence) {
  size_t n = text.size();
  const char* ch = text.data();
  for (size_t i = 0; i < n; ++i) {
    if (static_cast<unsigned char>(ch[i]) < 0x20) {
      *evidence = ch[i];
      return true;
    }
  }
  return false;
}


static bool _looks_like_url(const CString& text) {
  size_t n = text.size();
  const char* ch = text.data();
  if (n >= 8) {
    if (std::memcmp(ch, "https://", 8) == 0) return true;
    if (std::memcmp(ch, "http://", 7) == 0) return true;
    if (std::memcmp(ch, "file://", 7) == 0) return true;
    if (std::memcmp(ch, "ftp://", 6) == 0) return true;
  }
  return false;
}


// static bool _looks_like_glob(const CString& text) {
//   size_t n = static_cast<size_t>(text.size);
//   const char* ch = text.ch;
//   for (size_t i = 0; i < n; ++i) {
//     char c = ch[i];
//     if (c == '*' || c == '?' || c == '[' || c == ']') return true;
//   }
//   return false;
// }


static SourceVec _from_any(py::robj src, const GenericReader& rdr) {
  SourceVec out;
  if (src.is_string() || src.is_bytes()) {
    CString cstr = src.to_cstring();
    if (cstr.size() >= 4096) {
      D() << "Input is a string of length " << cstr.size()
          << ", treating it as raw text";
      out.emplace_back(new Source_Text(src));
      return out;
    }
    char c = '?';
    if (_has_control_characters(cstr, &c)) {
      D() << "Input contains '" << c << "', treating it as raw text";
      out.emplace_back(new Source_Text(src));
      return out;
    }
    if (_looks_like_url(cstr)) {
      D() << "Input is a URL";
      return _from_url(src, rdr);
    }
  }
  auto resolver = py::oobj::import("datatable.utils.fread", "_resolve_source_any");
  auto tempfiles = rdr.get_tempfiles();
  return _from_python(resolver.call({src, tempfiles}));
}




//------------------------------------------------------------------------------
// Process sources, and return the results
//------------------------------------------------------------------------------

static Error _multisrc_error() {
  return IOError() << "fread() input contains multiple sources";
}

static void emit_multisrc_warning() {
  auto w = IOWarning();
  w << "fread() input contains multiple sources, only the first will be used. "
       "Use iread() if you need to read all sources";
  w.emit_warning();
}

static void emit_badsrc_warning(const std::string& name, const Error& e) {
  auto w = IOWarning();
  w << "Could not read `" << name << "`: " << e.to_string();
  w.emit_warning();
}


// for fread
py::oobj MultiSource::read_single() {
  xassert(iteration_index_ == 0);
  if (sources_.empty()) {
    return py::Frame::oframe(new DataTable);
  }

  bool err = (reader_.multisource_strategy == FreadMultiSourceStrategy::Error);
  bool warn = (reader_.multisource_strategy == FreadMultiSourceStrategy::Warn);
  if (sources_.size() > 1 && err) throw _multisrc_error();

  py::oobj res = read_next();
  if (iteration_index_ < sources_.size()) {
    if (err) throw _multisrc_error();
    if (warn) emit_multisrc_warning();
  }
  return res;
}



py::oobj MultiSource::read_next()
{
  start:
  if (iteration_index_ >= sources_.size()) return py::oobj();

  py::oobj res;
  GenericReader new_reader(reader_);
  auto& src = sources_[iteration_index_];
  if (reader_.errors_strategy == IreadErrorHandlingStrategy::Error) {
    res = src->read(new_reader);
    py::Frame::cast_from(res)->set_source(src->name());
  }
  else {
    try {
      res = src->read(new_reader);
      py::Frame::cast_from(res)->set_source(src->name());
    }
    catch (const Error& e) {
      if (reader_.errors_strategy == IreadErrorHandlingStrategy::Warn) {
        emit_badsrc_warning(src->name(), e);
      }
      if (reader_.errors_strategy == IreadErrorHandlingStrategy::Store) {
        exception_to_python(e);
        PyObject* etype = nullptr;
        PyObject* evalue = nullptr;
        PyObject* etraceback = nullptr;
        PyErr_Fetch(&etype, &evalue, &etraceback);
        PyErr_NormalizeException(&etype, &evalue, &etraceback);
        if (etraceback) PyException_SetTraceback(evalue, etraceback);
        Py_XDECREF(etype);
        Py_XDECREF(etraceback);
        res = py::oobj::from_new_reference(evalue);
      }
    }
  }
  SourcePtr next = src->continuation();
  if (next) {
    sources_[iteration_index_] = std::move(next);
  } else {
    iteration_index_++;
  }
  if (!res) goto start;
  return res;
}




}}  // namespace dt::read
