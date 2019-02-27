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
#include "utils/parallel.h"
#include "datatablemodule.h"
#include "options.h"
#include "py_datatable.h"


//------------------------------------------------------------------------------
// read_csv()
//------------------------------------------------------------------------------

static py::PKArgs args_read_csv(
  1, 0, 0, false, false, {"reader"}, "gread",

R"(gread(reader)
--

Generic read function, similar to `fread` but supports other
file types, not just csv.
)");


static py::oobj read_csv(const py::PKArgs& args)
{
  py::robj pyreader = args[0];
  GenericReader rdr(pyreader);
  std::unique_ptr<DataTable> dtptr = rdr.read_all();
  return py::oobj::from_new_reference(
          py::Frame::from_datatable(dtptr.release()));
}



void py::DatatableModule::init_methods_csv() {
  ADD_FN(&read_csv, args_read_csv);
}
