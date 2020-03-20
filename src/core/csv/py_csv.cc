//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <stdlib.h>
#include "csv/reader.h"
#include "frame/py_frame.h"
#include "python/string.h"
#include "read/read_source.h"
#include "datatablemodule.h"
#include "options.h"

namespace py {


//------------------------------------------------------------------------------
// fread()
//------------------------------------------------------------------------------

static const char* doc_fread =
R"(fread(anysource=None, *, file=None, text=None, cmd=None, url=None,
         columns=None, sep=None, dec=".", max_nrows=None, header=None,
         na_strings=None, verbose=False, fill=False, encoding=None,
         skip_to_string=None, skip_to_line=None, skip_blank_lines=False,
         strip_whitespace=True, quotechar='"', save_to=None,
         tempdir=None, nthreads=None, logger=None)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)";

static PKArgs args_fread(
  1, 0, 22, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar", "save_to",
   "tempdir", "nthreads", "logger"
   },
  "fread", doc_fread);

static oobj fread(const PKArgs& args) {
  size_t k = 0;
  const Arg& arg_anysource  = args[k++];
  const Arg& arg_file       = args[k++];
  const Arg& arg_text       = args[k++];
  const Arg& arg_cmd        = args[k++];
  const Arg& arg_url        = args[k++];

  const Arg& arg_columns    = args[k++];
  const Arg& arg_sep        = args[k++];
  const Arg& arg_dec        = args[k++];
  const Arg& arg_maxnrows   = args[k++];
  const Arg& arg_header     = args[k++];
  const Arg& arg_nastrings  = args[k++];
  const Arg& arg_verbose    = args[k++];
  const Arg& arg_fill       = args[k++];
  const Arg& arg_encoding   = args[k++];
  const Arg& arg_skiptostr  = args[k++];
  const Arg& arg_skiptoline = args[k++];
  const Arg& arg_skipblanks = args[k++];
  const Arg& arg_stripwhite = args[k++];
  const Arg& arg_quotechar  = args[k++];
  const Arg& arg_saveto     = args[k++];
  const Arg& arg_tempdir    = args[k++];
  const Arg& arg_nthreads   = args[k++];
  const Arg& arg_logger     = args[k++];

  GenericReader rdr;
  rdr.init_logger(     arg_logger, arg_verbose);
  rdr.init_nthreads(   arg_nthreads);
  rdr.init_fill(       arg_fill);
  rdr.init_maxnrows(   arg_maxnrows);
  rdr.init_skiptoline( arg_skiptoline);
  rdr.init_sep(        arg_sep);
  rdr.init_dec(        arg_dec);
  rdr.init_quote(      arg_quotechar);
  rdr.init_header(     arg_header);
  rdr.init_nastrings(  arg_nastrings);
  rdr.init_skipstring( arg_skiptostr);
  rdr.init_stripwhite( arg_stripwhite);
  rdr.init_skipblanks( arg_skipblanks);
  rdr.init_columns(    arg_columns);
  rdr.init_tempdir(    arg_tempdir);
  (void) arg_saveto;
  (void) arg_encoding;

  auto read_sources = dt::read::resolve_sources(arg_anysource, arg_file, arg_text,
                                                arg_cmd, arg_url, rdr);
  if (read_sources.size() == 1) {
    return read_sources[0].read(rdr);
  }
  else {
    py::odict result_dict;
    for (auto& src : read_sources) {
      GenericReader ireader(rdr);
      result_dict.set(py::ostring(src.get_name()),
                      src.read(ireader));
    }
    return std::move(result_dict);
  }
}



void DatatableModule::init_methods_csv() {
  ADD_FN(&fread, args_fread);
}

} // namespace py
