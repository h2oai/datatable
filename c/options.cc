//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_OPTIONS_cc
#include "options.h"
#include "utils/omp.h"
#include "utils/pyobj.h"


namespace config
{

static int n_threads = 0;
PyObject* logger = nullptr;


int get_nthreads() {
  return n_threads;
}

void set_nthreads(int nth) {
  // Initialize `max_threads` only once, on the first run. This is because we
  // use `omp_set_num_threads` below, and once it was used,
  // `omp_get_max_threads` will return that number, and we won't be able to
  // know the "real" maximum number of threads.
  static const int max_threads = omp_get_max_threads();

  if (nth > max_threads) nth = max_threads;
  if (nth <= 0) nth += max_threads;
  if (nth <= 0) nth = 1;
  n_threads = nth;
  // Default number of threads that will be used in all `#pragma omp` calls
  // that do not use explicit `num_threads()` directive.
  omp_set_num_threads(nth);
}


PyObject* get_core_logger() {
  return logger;
}

void set_core_logger(PyObject* o) {
  if (o == Py_None) {
    logger = nullptr;
    Py_DECREF(o);
  } else {
    logger = o;
  }
}


PyObject* set_option(PyObject*, PyObject* args) {
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "OO", &arg1, &arg2)) return nullptr;
  PyObj arg_name(arg1);
  PyObj value(arg2);
  std::string name = arg_name.as_string();

  if (name == "nthreads")          set_nthreads(value.as_int32());
  else if (name == "core_logger")  set_core_logger(value.as_pyobject());
  else {
    throw ValueError() << "Unknown option `" << name << "`";
  }
  return none();
}


}; // namespace config
