//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "python/_all.h"
#include "python/string.h"
#include "python/xobject.h"
#include "datatablemodule.h"
#include "options.h"
namespace py {


//------------------------------------------------------------------------------
// config_option
//------------------------------------------------------------------------------

struct config_option : public XObject<config_option> {
  std::function<oobj()> getter;
  std::function<void(const py::Arg&)> setter;
  oobj name;
  oobj default_value;
  oobj docstring;
  Arg* arg;

  void m__init__(const PKArgs&) {}
  void m__dealloc__() {
    delete arg;
    name = nullptr;
    docstring = nullptr;
    default_value = nullptr;
    getter = nullptr;
    setter = nullptr;
    arg = nullptr;
  }

  oobj get(const PKArgs&) {
    return getter();
  }

  void set(const PKArgs& args) {
    arg->set(args[0].to_borrowed_ref());
    setter(*arg);
  }

  oobj get_name() const { return name; }
  oobj get_doc() const { return docstring; }
  oobj get_default() const { return default_value; }

  static void impl_init_type(XTypeMaker&);
};



static GSArgs args_name("name");
static GSArgs args_doc("doc");
static GSArgs args_default("default");
static PKArgs args___init__(0, 0, 0, false, false, {}, "__init__", nullptr);
static PKArgs args_get(0, 0, 0, false, false, {}, "get", nullptr);
static PKArgs args_set(1, 0, 0, false, false, {"x"}, "set", nullptr);


void config_option::impl_init_type(XTypeMaker& xt) {
  using co = config_option;
  xt.set_class_name("datatable.internal.Option");

  xt.add(CONSTRUCTOR(&co::m__init__, args___init__));
  xt.add(DESTRUCTOR(&co::m__dealloc__));
  xt.add(GETTER(&co::get_name, args_name));
  xt.add(GETTER(&co::get_doc, args_doc));
  xt.add(GETTER(&co::get_default, args_default));
  xt.add(METHOD(&co::get, args_get));
  xt.add(METHOD(&co::set, args_set));
}




}  // namespace py
//------------------------------------------------------------------------------
// dt::
//------------------------------------------------------------------------------
namespace dt {


static py::oobj dt_options = nullptr;

void use_options_store(py::oobj options) {
  dt_options = options;
}


void register_option(const char* name,
                     std::function<py::oobj()> getter,
                     std::function<void(const py::Arg&)> setter,
                     const char* docstring)
{
  xassert(dt_options);
  auto pytype = reinterpret_cast<PyObject*>(&py::config_option::type);
  auto v = PyObject_CallObject(pytype, nullptr);
  if (!v) throw PyError();
  py::oobj opt = py::oobj::from_new_reference(v);
  auto p = static_cast<py::config_option*>(v);
  p->name = py::ostring(name);
  p->default_value = getter();
  p->docstring = py::ostring(docstring);
  p->getter = std::move(getter);
  p->setter = std::move(setter);
  p->arg = new py::Arg(name);
  dt_options.invoke("register", opt);
}


py::oobj get_option(const char* name) {
  return dt_options.invoke("get", py::ostring(name));
}

void init_config_option(void* module) {
  py::config_option::init_type(static_cast<PyObject*>(module));
}




}  // namespace dt
