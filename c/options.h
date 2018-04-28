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

extern PyObject* logger;
extern int32_t nthreads;
extern size_t sort_insert_method_threshold;
extern size_t sort_thread_multiplier;
extern size_t sort_max_chunk_length;
extern int8_t sort_max_radix_bits;
extern int8_t sort_over_radix_bits;
extern int32_t sort_nthreads;
extern bool fread_anonymize;

void set_nthreads(int32_t n);
void set_core_logger(PyObject*);
void set_sort_insert_method_threshold(int64_t n);
void set_sort_thread_multiplier(int64_t n);
void set_sort_max_chunk_length(int64_t n);
void set_sort_max_radix_bits(int64_t n);
void set_sort_over_radix_bits(int64_t n);
void set_sort_nthreads(int32_t n);
void set_fread_anonymize(int8_t v);


DECLARE_FUNCTION(
  set_option,
  "set_option(name, value)\n\n"
  "Set core option `name` to the given `value`. The `name` must be a string,\n"
  "and it must be one of the recognizable option names. If not, an exception\n"
  "will be raised. The allowed `value`s depend on the option being set.",
  dt_OPTIONS_cc)

};

#endif
