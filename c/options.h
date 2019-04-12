//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_OPTIONS_h
#define dt_OPTIONS_h
#include <functional>  // std::function
#include <string>      // std::string
#include "python/ext_type.h"
namespace config {



extern PyObject* logger;
extern size_t sort_insert_method_threshold;
extern size_t sort_thread_multiplier;
extern size_t sort_max_chunk_length;
extern uint8_t sort_max_radix_bits;
extern uint8_t sort_over_radix_bits;
extern int32_t sort_nthreads;
extern bool fread_anonymize;

void set_core_logger(PyObject*);
void set_sort_insert_method_threshold(int64_t n);
void set_sort_thread_multiplier(int64_t n);
void set_sort_max_chunk_length(int64_t n);
void set_sort_max_radix_bits(int64_t n);
void set_sort_over_radix_bits(int64_t n);
void set_sort_nthreads(int32_t n);
void set_fread_anonymize(int8_t v);


void register_option(const char* name,
                     std::function<py::oobj()> getter,
                     std::function<void(py::oobj)> setter,
                     const char* docstring);

void use_options_store(py::oobj options);


}
//------------------------------------------------------------------------------
// config_option
//------------------------------------------------------------------------------
namespace py {

struct config_option : public PyObject {
  std::function<oobj()> getter;
  std::function<void(oobj)> setter;
  oobj name;
  oobj default_value;
  oobj docstring;
  size_t : 64;

  class Type : public ExtType<config_option> {
    public:
      static PKArgs args___init__;
      static const char* classname();
      static const char* classdoc();
      static bool is_subclassable();
      static void init_methods_and_getsets(Methods&, GetSetters&);
  };

  void m__init__(PKArgs&);
  void m__dealloc__();
  oobj get(const PKArgs&);
  void set(const PKArgs&);

  oobj get_name() const;
  oobj get_doc() const;
  oobj get_default() const;
};


}  // namespace py
#endif
