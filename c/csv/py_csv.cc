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
// read_csv()
//------------------------------------------------------------------------------

static PKArgs args_read_csv(
  1, 0, 0, false, false, {"reader"}, "gread",

R"(gread(reader)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)");


static oobj read_csv(const PKArgs& args) {
  robj pyreader = args[0];
  GenericReader rdr(pyreader);
  return rdr.read_all();
}



void DatatableModule::init_methods_csv() {
  ADD_FN(&read_csv, args_read_csv);
}

} // namespace py
