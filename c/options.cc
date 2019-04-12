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


namespace config
{

PyObject* logger = nullptr;
size_t sort_insert_method_threshold = 64;
size_t sort_thread_multiplier = 2;
size_t sort_max_chunk_length = 1 << 20;
uint8_t sort_max_radix_bits = 16;
uint8_t sort_over_radix_bits = 16;
int32_t sort_nthreads = 1;


static int32_t normalize_nthreads(int32_t nth) {
  if (nth <= 0) nth += static_cast<int32_t>(dt::get_hardware_concurrency());
  if (nth <= 0) nth = 1;
  return nth;
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




static py::PKArgs args_set_option(
    2, 0, 0, false, false,
    {"name", "value"}, "set_option", nullptr);


static py::oobj set_option(const py::PKArgs& args) {
  std::string name = args[0].to_string();
  py::robj value = args[1];

  if (name == "sort.insert_method_threshold") {
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

  if (name == "sort.insert_method_threshold") {
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

  } else {
    throw ValueError() << "Unknown option `" << name << "`";
  }
}



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


}; // namespace config
namespace py {



void DatatableModule::init_methods_options() {
  ADD_FN(&config::get_option, config::args_get_option);
  ADD_FN(&config::set_option, config::args_set_option);
}


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
