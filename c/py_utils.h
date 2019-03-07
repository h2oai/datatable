//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------


/*******************************************************************************
 * Collection of macros / helper functions to facilitate translation from
 * Python to C.
 ******************************************************************************/
#ifndef dt_PYUTILS_H
#define dt_PYUTILS_H
#include <Python.h>
#include "utils.h"
#include "utils/exceptions.h"

namespace config {
  extern PyObject* logger;
};
extern double logger_timer;
extern char logger_msg[];
void log_call(const char* msg);




PyObject* none(void);
PyObject* incref(PyObject *x);
PyObject* decref(PyObject *x);



#endif
