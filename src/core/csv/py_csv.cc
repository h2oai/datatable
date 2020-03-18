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
  1, 0, 0, false, false, {"reader"}, "gread",

R"(gread(reader)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)");

static oobj gread(const PKArgs& args) {
  robj pyreader = args[0];
  oobj source = pyreader.get_attr("_src");
  oobj logger = pyreader.get_attr("_logger");
  oobj tempfiles = pyreader.get_attr("_tempfiles");

  if (!logger.is_none()) {
    logger.invoke("debug", ostring("[1] Prepare for reading"));
  }
  auto resolve_source = oobj::import("datatable.fread", "_resolve_source");
  auto res_tuple = resolve_source.call({source, tempfiles, logger}).to_otuple();
  auto sources = res_tuple[0];
  auto result = res_tuple[1];
  if (result.is_none()) {
    GenericReader rdr(pyreader, sources);
    return rdr.read_all();
  }
  if (result.is_list_or_tuple()) {
    py::olist result_list = result.to_pylist();
    py::odict result_dict;
    for (size_t i = 0; i < result_list.size(); ++i) {
      auto entry = result_list[i].to_otuple();
      auto isources = entry[0];
      auto iresult = entry[1];
      auto isrc = isources.to_otuple()[0];
      if (iresult.is_none()) {
        GenericReader rdr(pyreader, isources);
        result_dict.set(isrc, rdr.read_all());
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
