//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------

#include "options.h"                // dt::register_option
#include "utils/logger.h"
#include <iostream>

namespace dt {
namespace logger {


//------------------------------------------------------------------------------
// dt.options.logger.anonymize
//------------------------------------------------------------------------------

bool anonymize = false;

static void init_option_anonymize() {
  dt::register_option(
    "logger.anonymize",
    []{ return py::obool(anonymize); },
    [](const py::Arg& value){ anonymize = value.to_bool_strict(); },
    "When True, logger will anonymize the data."
  );
}


//------------------------------------------------------------------------------
// dt.options.logger.logger_obj
//------------------------------------------------------------------------------

// This cannot be `py::oobj`, because then the static variable will be destroyed
// on program's exit. However, at that point the Python runtime will have be
// already shut down, and trying to garbage-collect a python object at that
// moment leads to a segfault.
PyObject* object = nullptr;

static py::oobj get_object() {
  return object? py::oobj(object) : py::None();
}


static void set_object(const py::Arg& value) {
  py::oobj py_obj = value.to_oobj();
  PyObject* object_in = py_obj.to_borrowed_ref();


  PyObject* module = PyImport_ImportModule("logging");
  PyObject* class_in = PyObject_GetAttrString(module, "Handler");

  int res = PyObject_IsInstance(object_in, class_in);

  if (res == 0) {
    throw TypeError() << "Logger object must be an instance or subclass of "
                         "`logging.Handler`";
  }

  Py_XSETREF(object,
             value.is_none()? nullptr : std::move(py_obj).release());
}


static void init_option_object(){
  dt::register_option(
    "logger.object",
    get_object,
    set_object,
    "If None, then the built-in logger object is used.\n"
    "Otherwise, this value specifies a Python object to be used as a logger."
  );
}


//------------------------------------------------------------------------------
// Init
//------------------------------------------------------------------------------

void init_options() {
  init_option_anonymize();
  init_option_object();
}


}} // namespace dt::logger
