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
#include <cstring>   // std::memcmp
#include <string>
#include <vector>
#include "csv/reader.h"
#include "python/_all.h"
#include "read/read_source.h"
#include "datatablemodule.h"
namespace dt {
namespace read {

// forward-declare
static void _check_src_args(const py::PKArgs&);
static ReadSource _resolve_source_any(const py::Arg&, GenericReader&);
static ReadSource _resolve_source_file(const py::Arg&, GenericReader&);
static ReadSource _resolve_source_text(const py::Arg&, GenericReader&);
static ReadSource _resolve_source_cmd(const py::Arg&, GenericReader&);
static ReadSource _resolve_source_url(const py::Arg&, GenericReader&);
static bool _has_control_characters(const CString& str);
/*
static bool _looks_like_glob(const CString& str);
static bool _looks_like_url(const CString& str);
*/


//------------------------------------------------------------------------------
// zread()
//------------------------------------------------------------------------------

static py::PKArgs args_zread(
  1, 0, 21, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings", "verbose",
   "fill", "encoding", "skip_to_string", "skip_to_line", "skip_blank_lines",
   "strip_whitespace", "quotechar", "save_to", "nthreads", "logger"},
  "zread",

R"(zread(anysource, *, file=None, text=None, cmd=None, url=None,
      columns=None, sep=None, dec=".", max_nrows=None, header=None,
      na_strings=None, verbose=False, fill=False, encoding=None,
      skip_to_string=None, skip_to_line=None, skip_blank_lines=False,
      strip_whitespace=True, quotechar='"', save_to=None,
      nthreads=None, logger=None)
--

New (prototype) fread function
)");

static py::oobj zread(const py::PKArgs& args)
{
  _check_src_args(args);
  const py::Arg& src_any  = args[0];
  const py::Arg& src_file = args[1];
  const py::Arg& src_text = args[2];
  const py::Arg& src_cmd  = args[3];
  const py::Arg& src_url  = args[4];
  const py::Arg& arg_columns          = args[5];
  const py::Arg& arg_sep              = args[6];
  const py::Arg& arg_dec              = args[7];
  const py::Arg& arg_max_nrows        = args[8];
  const py::Arg& arg_header           = args[9];
  const py::Arg& arg_na_strings       = args[10];
  const py::Arg& arg_verbose          = args[11];
  const py::Arg& arg_fill             = args[12];
  const py::Arg& arg_encoding         = args[13];
  const py::Arg& arg_skip_to_string   = args[14];
  const py::Arg& arg_skip_to_line     = args[15];
  const py::Arg& arg_skip_blank_lines = args[16];
  const py::Arg& arg_strip_whitespace = args[17];
  const py::Arg& arg_quotechar        = args[18];
  const py::Arg& arg_save_to          = args[19];
  const py::Arg& arg_nthreads         = args[20];
  const py::Arg& arg_logger           = args[21];

  GenericReader gr;
  gr.init_verbose(arg_verbose);
  gr.init_logger(arg_logger);
  gr.init_nthreads(arg_nthreads);
  gr.init_fill(arg_fill);
  gr.init_maxnrows(arg_max_nrows);
  gr.init_skiptoline(arg_skip_to_line);
  gr.init_sep(arg_sep);
  gr.init_dec(arg_dec);
  gr.init_quote(arg_quotechar);
  gr.init_header(arg_header);
  gr.init_nastrings(arg_na_strings);
  gr.init_skipstring(arg_skip_to_string);
  gr.init_stripwhite(arg_strip_whitespace);
  gr.init_skipblanks(arg_skip_blank_lines);
  gr.init_columns(arg_columns);
  (void) arg_save_to;
  (void) arg_encoding;

  ReadSource source =
      src_any.is_defined() ? _resolve_source_any(src_any, gr) :
      src_file.is_defined()? _resolve_source_file(src_file, gr) :
      src_text.is_defined()? _resolve_source_text(src_text, gr) :
      src_cmd.is_defined() ? _resolve_source_cmd(src_cmd, gr) :
                             _resolve_source_url(src_url, gr);
  return source.read_one();
}




//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static void _check_src_args(const py::PKArgs& args) {
  bool defined_any  = args[0].is_defined();
  bool defined_file = args[1].is_defined();
  bool defined_text = args[2].is_defined();
  bool defined_cmd  = args[3].is_defined();
  bool defined_url  = args[4].is_defined();
  int total = defined_any + defined_file + defined_text + defined_cmd +
              defined_url;
  if (total == 1) return;  // normal

  if (total == 0) {
    throw ValueError()
        << "No input source for fread was given. Please specify one of "
           "the parameters `file`, `text`, `url`, or `cmd`";
  }
  std::vector<const char*> extra_args;
  if (defined_file) extra_args.push_back(args[1].short_name());
  if (defined_text) extra_args.push_back(args[2].short_name());
  if (defined_cmd)  extra_args.push_back(args[3].short_name());
  if (defined_url)  extra_args.push_back(args[4].short_name());
  if (defined_any) {
    throw ValueError()
        << "When an unnamed argument is passed to fread, it is invalid "
           "to also provide the `" << extra_args[0] << "` parameter";
  } else {
    throw ValueError()
        << "Both parameters `" << extra_args[0] << "` and `" << extra_args[1]
        << "` cannot be passed to fread simultaneously";
  }
}


// Return true if `text` has any characters from C0 range.
static bool _has_control_characters(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  for (size_t i = 0; i < n; ++i) {
    if (static_cast<unsigned char>(ch[i]) < 0x20) {
      return true;
    }
  }
  return false;
}


#if 0
static bool _looks_like_url(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  if (n >= 8) {
    if (std::memcmp(ch, "https://", 8) == 0) return true;
    if (std::memcmp(ch, "http://", 7) == 0) return true;
    if (std::memcmp(ch, "file://", 7) == 0) return true;
    if (std::memcmp(ch, "ftp://", 6) == 0) return true;
  }
  return false;
}


static bool _looks_like_glob(const CString& text) {
  size_t n = static_cast<size_t>(text.size);
  const char* ch = text.ch;
  for (size_t i = 0; i < n; ++i) {
    char c = ch[i];
    if (c == '*' || c == '?' || c == '[' || c == ']') return true;
  }
  return false;
}
#endif


static ReadSource _resolve_source_any(const py::Arg& src, GenericReader& gr) {
  (void)gr;
  xassert(src.is_defined());
  auto srcobj = src.to_oobj();
  if (src.is_string() || src.is_bytes()) {
    CString cstr = src.to_cstring();
    if (cstr.size >= 4096 || _has_control_characters(cstr)) {
      return ReadSource::from_text(srcobj);
    }
    // if (_looks_like_url(cstr)) {
    //   return ReadSource::from_url(srcobj);
    // }
  }
  #if 0
    if isinstance(src, (str, bytes)):
        if len(src) >= 4096 or _has_control_characters(src):
            return FreadSource_Text(src, "<text>", params)
        if isinstance(src, str):
            if _url_regex.match(src):
                return FreadSource_Url(src, src, params)
            if _glob_regex.search(src):
                return FreadSource_Glob(src, src, params)
        p = pathlib.Path(src)
        return FreadSource_Path(p, params)

    if isinstance(src, os.PathLike):
        return FreadSource_Path(src, params)

    if hasattr(src, "read"):
        return FreadSource.from_fileobj(src)

    if isinstance(src, (list, tuple)):
        return FreadSource.from_list(src)

  #endif
  throw TypeError() << "Unknown type for the first argument in fread: "
                    << src.typeobj();
}


static ReadSource _resolve_source_file(const py::Arg&, GenericReader&) {
  throw NotImplError();
}

static ReadSource _resolve_source_text(const py::Arg&, GenericReader&) {
  throw NotImplError();
}

static ReadSource _resolve_source_cmd(const py::Arg&, GenericReader&) {
  throw NotImplError();
}

static ReadSource _resolve_source_url(const py::Arg&, GenericReader&) {
  throw NotImplError();
}





}}  // namespace dt::read


//------------------------------------------------------------------------------
// Export into `_datatable` module
//------------------------------------------------------------------------------

void py::DatatableModule::init_methods_zread() {
  ADD_FN(&dt::read::zread, dt::read::args_zread);
}

