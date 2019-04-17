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
#include "python/ext_type.h"
#include "python/obj.h"
#include "python/string.h"
#include "datatablemodule.h"
#include "options.h"
namespace dt {


static py::oobj dt_options = nullptr;

void use_options_store(py::oobj options) {
  dt_options = options;
}


void register_option(const char* name,
                     std::function<py::oobj()> getter,
                     std::function<void(py::oobj)> setter,
                     const char* docstring)
{
  xassert(dt_options);
  auto pytype = reinterpret_cast<PyObject*>(&py::config_option::Type::type);
  auto v = PyObject_CallObject(pytype, nullptr);
  if (!v) throw PyError();
  auto p = static_cast<py::config_option*>(v);
  p->name = py::ostring(name);
  p->default_value = getter();
  p->docstring = py::ostring(docstring);
  p->getter = std::move(getter);
  p->setter = std::move(setter);
  py::oobj opt = py::oobj::from_new_reference(v);
  dt_options.invoke("register", py::otuple{ opt });
}


py::oobj get_option(const char* name) {
  return dt_options.invoke("get", py::ostring(name));
}



}  // namespace dt
namespace py {
//------------------------------------------------------------------------------
// config_option
//------------------------------------------------------------------------------

static GSArgs args_name("name");
static GSArgs args_doc("doc");
static GSArgs args_default("default");

oobj config_option::get_name() const { return name; }
oobj config_option::get_doc() const { return docstring; }
oobj config_option::get_default() const { return default_value; }


void config_option::m__init__(PKArgs&) {}

void config_option::m__dealloc__() {
  name = nullptr;
  docstring = nullptr;
  default_value = nullptr;
  getter = nullptr;
  setter = nullptr;
}


static PKArgs args_get(0, 0, 0, false, false, {}, "get", nullptr);
static PKArgs args_set(1, 0, 0, false, false, {"x"}, "set", nullptr);

oobj config_option::get(const PKArgs&) { return getter(); }
void config_option::set(const PKArgs& args) { setter(args[0].to_oobj()); }



//------------------------------------------------------------------------------
// config_option::Type
//------------------------------------------------------------------------------

const char* config_option::Type::classname() {
  return "datatable.internal.Option";
}

const char* config_option::Type::classdoc() {
  return nullptr;
}

bool config_option::Type::is_subclassable() {
  return false;
}

void config_option::Type::init_methods_and_getsets(
    Methods& mm, GetSetters& gs)
{
  using co = config_option;
  ADD_GETTER(gs, &co::get_name, args_name);
  ADD_GETTER(gs, &co::get_doc, args_doc);
  ADD_GETTER(gs, &co::get_default, args_default);
  ADD_METHOD(mm, &co::get, args_get);
  ADD_METHOD(mm, &co::set, args_set);
}


PKArgs config_option::Type::args___init__(
  0, 0, 0, false, false, {}, "__init__", nullptr);


}  // namespace py
