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
#include "python/ext_type.h"
#include "python/obj.h"
namespace dt {


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
