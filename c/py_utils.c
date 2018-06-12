//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>
#include <string.h>  // memcpy
#include "py_datatable.h"
#include "py_utils.h"

double logger_timer;
char logger_msg[1000];

/**
 * Create and return a new instance of python's None object
 * (actually, this reuses Py_None singleton, increasing its REFCNT)
 */
PyObject* none(void) {
    Py_INCREF(Py_None);
    return Py_None;
}


/**
 * Increment reference count of PyObject `x`, and then return it.
 */
PyObject* incref(PyObject *x) {
    Py_XINCREF(x);
    return x;
}


/**
 * Decrement reference count of PyObject `x`, and then return NULL.
 */
PyObject* decref(PyObject *x) {
    Py_XDECREF(x);
    return nullptr;
}



void log_call(const char* msg) {
  if (!config::logger) return;
  PyObject_CallMethod(config::logger, "info", "s", msg);
}
