//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_OPTIONS_cc
#include "options.h"
#include "utils/exceptions.h"
#include "utils/omp.h"
#include "utils/pyobj.h"


namespace config
{

PyObject* logger = nullptr;
int nthreads = 1;
size_t sort_insert_method_threshold = 64;
size_t sort_thread_multiplier = 2;
size_t sort_max_chunk_length = 1 << 20;
int8_t sort_max_radix_bits = 16;
int8_t sort_over_radix_bits = 16;


void set_nthreads(int nth) {
  // Initialize `max_threads` only once, on the first run. This is because we
  // use `omp_set_num_threads` below, and once it was used,
  // `omp_get_max_threads` will return that number, and we won't be able to
  // know the "real" maximum number of threads.
  static const int max_threads = omp_get_max_threads();

  if (nth > max_threads) nth = max_threads;
  if (nth <= 0) nth += max_threads;
  if (nth <= 0) nth = 1;
  nthreads = nth;
  // Default number of threads that will be used in all `#pragma omp` calls
  // that do not use explicit `num_threads()` directive.
  omp_set_num_threads(nth);
}


void set_core_logger(PyObject* o) {
  if (o == Py_None) {
    logger = nullptr;
    Py_DECREF(o);
  } else {
    logger = o;
  }
}

void set_sort_insert_method_threshold(int64_t n) {
  if (n < 0) n = 0;
  sort_insert_method_threshold = static_cast<size_t>(n);
}

void set_sort_thread_multiplier(int64_t n) {
  if (n < 1) n = 1;
  sort_thread_multiplier = static_cast<size_t>(n);
}

void set_sort_max_chunk_length(int64_t n) {
  if (n < 1) n = 1;
  sort_max_chunk_length = static_cast<size_t>(n);
}

void set_sort_max_radix_bits(int64_t n) {
  sort_max_radix_bits = static_cast<int8_t>(n);
  if (sort_max_radix_bits <= 0)
    throw ValueError() << "Invalid sort.max_radix_bits parameter: " << n;
}

void set_sort_over_radix_bits(int64_t n) {
  sort_over_radix_bits = static_cast<int8_t>(n);
  if (sort_over_radix_bits <= 0)
    throw ValueError() << "Invalid sort.over_radix_bits parameter: " << n;
}



PyObject* set_option(PyObject*, PyObject* args) {
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "OO", &arg1, &arg2)) return nullptr;
  PyObj arg_name(arg1);
  PyObj value(arg2);
  std::string name = arg_name.as_string();

  if (name == "nthreads") {
    set_nthreads(value.as_int32());

  } else if (name == "sort.insert_method_threshold") {
    set_sort_insert_method_threshold(value.as_int64());

  } else if (name == "sort.thread_multiplier") {
    set_sort_thread_multiplier(value.as_int64());

  } else if (name == "sort.max_chunk_length") {
    set_sort_max_chunk_length(value.as_int64());

  } else if (name == "sort.max_radix_bits") {
    set_sort_max_radix_bits(value.as_int64());

  } else if (name == "sort.over_radix_bits") {
    set_sort_over_radix_bits(value.as_int64());

  } else if (name == "core_logger") {
    set_core_logger(value.as_pyobject());

  } else {
    throw ValueError() << "Unknown option `" << name << "`";
  }
  return none();
}


}; // namespace config
