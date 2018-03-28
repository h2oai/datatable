//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef CSV_PY_CSV_h
#define CSV_PY_CSV_h
#include <Python.h>
#include "py_utils.h"

#define CLSNAME dt

DECLARE_FUNCTION(
  write_csv,
  "write_csv()\n\n",
  CSV_PY_CSV_cc)


DECLARE_FUNCTION(
  gread,
  "gread(reader)\n\n"
  "'Generic read' function, similar to `fread` but supports other file \n"
  "types, not just csv.\n",
  CSV_PY_CSV_cc)

#undef CLSNAME
#endif
