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
  (void) arg_saveto;
  (void) arg_encoding;

  oobj source = otuple({arg_anysource.to_oobj_or_none(),
                        arg_file.to_oobj_or_none(),
                        arg_text.to_oobj_or_none(),
                        arg_cmd.to_oobj_or_none(),
                        arg_url.to_oobj_or_none()});
  oobj logger = rdr.get_logger();
  oobj clsTempFiles = oobj::import("datatable.utils.fread", "TempFiles");
  oobj tempdir = arg_tempdir.to_oobj_or_none();
  oobj tempfiles = logger? clsTempFiles.call({tempdir, logger})
                         : clsTempFiles.call(tempdir);

  if (logger) {
    logger.invoke("debug", ostring("[1] Prepare for reading"));
  }
  auto resolve_source = oobj::import("datatable.utils.fread", "_resolve_source");
  auto res_tuple = resolve_source.call({source, tempfiles}).to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];
  if (result.is_none()) {
    return rdr.read_all(sources);
  }
  if (result.is_list_or_tuple()) {
    py::olist result_list = result.to_pylist();
    py::odict result_dict;
    for (size_t i = 0; i < result_list.size(); ++i) {
      GenericReader ireader(rdr);
      auto entry = result_list[i].to_otuple();
      auto isources = entry[0];
      auto iresult = entry[1];
      auto isrc = isources.to_otuple()[0];
      if (iresult.is_none()) {
        result_dict.set(isrc, ireader.read_all(isources));
      } else {
        result_dict.set(isrc, iresult);
      }
    }
    return oobj(std::move(result_dict));
  }
  return result;
}



void DatatableModule::init_methods_csv() {
  ADD_FN(&fread, args_fread);
}

} // namespace py
