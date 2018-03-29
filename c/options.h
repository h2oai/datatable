//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_OPTIONS_h
#define dt_OPTIONS_h
#include <Python.h>
#include "py_utils.h"

namespace config {

int get_nthreads();
void set_nthreads(int nth);

extern PyObject* logger;
PyObject* get_core_logger();
void set_core_logger(PyObject*);
};



DECLARE_FUNCTION(
  set_nthreads,
  "set_nthreads(nth)\n\n",
  dt_OPTIONS_cc)

DECLARE_FUNCTION(
  set_core_logger,
  "set_core_logger(logger)\n\n",
  dt_OPTIONS_cc)


#endif
