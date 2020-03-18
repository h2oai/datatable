//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
// gread()
//------------------------------------------------------------------------------

static PKArgs args_gread(
  1, 0, 22, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar", "save_to",
   "tempdir", "nthreads", "logger"
   },
  "gread",

R"(gread(reader)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)");

static oobj gread(const PKArgs& args) {
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

  oobj source = otuple({arg_anysource.to_oobj(),
                        arg_file.to_oobj(),
                        arg_text.to_oobj(),
                        arg_cmd.to_oobj(),
                        arg_url.to_oobj()});
  oobj logger = rdr.get_logger();
  oobj clsTempFiles = oobj::import("datatable.fread", "TempFiles");
  oobj tempdir = arg_tempdir.to_oobj();
  oobj tempfiles = logger? clsTempFiles.call({tempdir, logger})
                         : clsTempFiles.call(tempdir);

  if (logger) {
    logger.invoke("debug", ostring("[1] Prepare for reading"));
  }
  auto resolve_source = oobj::import("datatable.fread", "_resolve_source");
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
  ADD_FN(&gread, args_gread);
}

} // namespace py
