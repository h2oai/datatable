//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "parallel/api.h"
#include "python/_all.h"
#include "python/ext_type.h"
#include "python/obj.h"
#include "python/string.h"
#include "utils/parallel.h"
#include "datatablemodule.h"
#include "options.h"


namespace config
{

PyObject* logger = nullptr;
int32_t nthreads = 1;
size_t sort_insert_method_threshold = 64;
size_t sort_thread_multiplier = 2;
size_t sort_max_chunk_length = 1 << 20;
uint8_t sort_max_radix_bits = 16;
uint8_t sort_over_radix_bits = 16;
int32_t sort_nthreads = 1;
bool fread_anonymize = false;
int64_t frame_names_auto_index = 0;
std::string frame_names_auto_prefix = "C";
bool display_interactive = false;
bool display_interactive_hint = true;


int32_t normalize_nthreads(int32_t nth) {
  // Initialize `max_threads` only once, on the first run. This is because we
  // use `omp_set_num_threads` below, and once it was used,
  // `omp_get_max_threads` will return that number, and we won't be able to
  // know the "real" maximum number of threads.
  static const int max_threads = omp_get_max_threads();

  if (nth <= 0) nth += max_threads;
  if (nth <= 0) nth = 1;
  return nth;
}

void set_nthreads(int32_t n) {
  n = normalize_nthreads(n);
  nthreads = n;
  sort_nthreads = n;
  // Default number of threads that will be used in all `#pragma omp` calls
  // that do not use explicit `num_threads()` directive.
  omp_set_num_threads(n);

  dt::get_thread_pool().resize(static_cast<size_t>(n));
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
  sort_max_radix_bits = static_cast<uint8_t>(n);
  if (sort_max_radix_bits <= 0)
    throw ValueError() << "Invalid sort.max_radix_bits parameter: " << n;
}

void set_sort_over_radix_bits(int64_t n) {
  sort_over_radix_bits = static_cast<uint8_t>(n);
  if (sort_over_radix_bits <= 0)
    throw ValueError() << "Invalid sort.over_radix_bits parameter: " << n;
}

void set_sort_nthreads(int32_t n) {
  sort_nthreads = normalize_nthreads(n);
}

void set_fread_anonymize(int8_t v) {
  fread_anonymize = v;
}



static py::PKArgs args_set_option(
    2, 0, 0, false, false,
    {"name", "value"}, "set_option", nullptr);


static py::oobj set_option(const py::PKArgs& args) {
  std::string name = args[0].to_string();
  py::robj value = args[1];

  if (name == "nthreads") {
    set_nthreads(value.to_int32_strict());

  } else if (name == "sort.insert_method_threshold") {
    set_sort_insert_method_threshold(value.to_int64_strict());

  } else if (name == "sort.thread_multiplier") {
    set_sort_thread_multiplier(value.to_int64_strict());

  } else if (name == "sort.max_chunk_length") {
    set_sort_max_chunk_length(value.to_int64_strict());

  } else if (name == "sort.max_radix_bits") {
    set_sort_max_radix_bits(value.to_int64_strict());

  } else if (name == "sort.over_radix_bits") {
    set_sort_over_radix_bits(value.to_int64_strict());

  } else if (name == "sort.nthreads") {
    set_sort_nthreads(value.to_int32_strict());

  } else if (name == "core_logger") {
    set_core_logger(py::oobj(value).release());

  } else if (name == "fread.anonymize") {
    set_fread_anonymize(value.to_bool_strict());

  } else if (name == "frame.names_auto_index") {
    frame_names_auto_index = value.to_int64_strict();

  } else if (name == "frame.names_auto_prefix") {
    frame_names_auto_prefix = value.to_string();

  } else if (name == "display.interactive") {
    display_interactive = value.to_bool_strict();

  } else if (name == "display.interactive_hint") {
    display_interactive_hint = value.to_bool_strict();

  } else {
    // throw ValueError() << "Unknown option `" << name << "`";
  }
  return py::None();
}



static py::PKArgs args_get_option(
    1, 0, 0, false, false,
    {"name"}, "get_option", nullptr);


static py::oobj get_option(const py::PKArgs& args) {
  std::string name = args[0].to_string();

  if (name == "nthreads") {
    return py::oint(nthreads);

  } else if (name == "sort.insert_method_threshold") {
    return py::oint(sort_insert_method_threshold);

  } else if (name == "sort.thread_multiplier") {
    return py::oint(sort_thread_multiplier);

  } else if (name == "sort.max_chunk_length") {
    return py::oint(sort_max_chunk_length);

  } else if (name == "sort.max_radix_bits") {
    return py::oint(sort_max_radix_bits);

  } else if (name == "sort.over_radix_bits") {
    return py::oint(sort_over_radix_bits);

  } else if (name == "sort.nthreads") {
    return py::oint(sort_nthreads);

  } else if (name == "core_logger") {
    return logger? py::oobj(logger) : py::None();

  } else if (name == "fread.anonymize") {
    return py::obool(fread_anonymize);

  } else if (name == "frame.names_auto_index") {
    return py::oint(frame_names_auto_index);

  } else if (name == "frame.names_auto_prefix") {
    return py::ostring(frame_names_auto_prefix);

  } else if (name == "display.interactive") {
    return py::obool(display_interactive);

  } else if (name == "display.interactive_hint") {
    return py::obool(display_interactive_hint);

  } else {
    throw ValueError() << "Unknown option `" << name << "`";
  }
}



}; // namespace config


void py::DatatableModule::init_methods_options() {
  ADD_FN(&config::get_option, config::args_get_option);
  ADD_FN(&config::set_option, config::args_set_option);
}


