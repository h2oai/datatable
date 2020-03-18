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
  1, 0, 21, false, false,
  {"anysource", "file", "text", "cmd", "url",
   "columns", "sep", "dec", "max_nrows", "header", "na_strings",
   "verbose", "fill", "encoding", "skip_to_string", "skip_to_line",
   "skip_blank_lines", "strip_whitespace", "quotechar", "save_to",
   "nthreads", "logger"
   },
  "gread",

R"(gread(reader)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)");

static oobj gread(const PKArgs& args) {
  const Arg& arg_anysource  = args[0];
  const Arg& arg_file       = args[1];
  const Arg& arg_text       = args[2];
  const Arg& arg_cmd        = args[3];
  const Arg& arg_url        = args[4];

  const Arg& arg_columns    = args[5];
  const Arg& arg_sep        = args[6];
  const Arg& arg_dec        = args[7];
  const Arg& arg_maxnrows   = args[8];
  const Arg& arg_header     = args[9];
  const Arg& arg_nastrings  = args[10];
  const Arg& arg_verbose    = args[11];
  const Arg& arg_fill       = args[12];
  const Arg& arg_encoding   = args[13];
  const Arg& arg_skiptostr  = args[14];
  const Arg& arg_skiptoline = args[15];
  const Arg& arg_skipblanks = args[16];
  const Arg& arg_stripwhite = args[17];
  const Arg& arg_quotechar  = args[18];
  const Arg& arg_saveto     = args[19];
  const Arg& arg_nthreads   = args[20];
  const Arg& arg_logger     = args[21];

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

  oobj source = otuple({arg_anysource.to_oobj(),
                        arg_file.to_oobj(),
                        arg_text.to_oobj(),
                        arg_cmd.to_oobj(),
                        arg_url.to_oobj()});
  oobj logger = rdr.get_logger();
  oobj clsTempFiles = oobj::import("datatable.fread", "TempFiles");
  oobj tempfiles = logger? clsTempFiles.call({py::None(), logger})
                         : clsTempFiles.call();

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
