//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_PYTHON_EXT_MODULE_h
#define dt_PYTHON_EXT_MODULE_h
#include <vector>
#include <Python.h>
#include "python/args.h"
#include "python/obj.h"
#include "utils/exceptions.h"

namespace py {


/**
 *
 */
template <class T>
class ExtModule {
  private:
    PyModuleDef module_def;
    std::vector<PyMethodDef> methods;
    PyObject* pymodule;

  public:
    PyObject* init();

    void init_methods() {}

    template <py::oobj (*F)(const PKArgs&), PKArgs& ARGS>
    void add(const char* name, const char* doc);
    template <void (*F)(const PKArgs&), PKArgs& ARGS>
    void add(const char* name, const char* doc);
    void add(PyMethodDef);

  private:
    PyMethodDef* get_methods();
};


template <typename A, void (*F)(const A&), A& ARGS>
PyObject* _safe_function0(PyObject*, PyObject* args, PyObject* kwds) {
  try {
    ARGS.bind(args, kwds);
    (*F)(ARGS);
    Py_RETURN_NONE;
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

template <typename A, oobj (*F)(const A&), A& ARGS>
PyObject* _safe_function1(PyObject*, PyObject* args, PyObject* kwds) {
  try {
    ARGS.bind(args, kwds);
    oobj res = (*F)(ARGS);
    return std::move(res).release();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


template <class T>
void ExtModule<T>::add(PyMethodDef def) {
  methods.push_back(def);
}

template <class T>
template <py::oobj (*F)(const PKArgs&), PKArgs& ARGS>
void ExtModule<T>::add(const char* name, const char* doc) {
  ARGS.set_class_name(static_cast<T*>(this)->name());
  ARGS.set_function_name(name);
  methods.push_back(PyMethodDef {
    name,
    reinterpret_cast<PyCFunction>(&_safe_function1<PKArgs, F, ARGS>),
    METH_VARARGS | METH_KEYWORDS,
    doc
  });
}

template <class T>
template <void (*F)(const PKArgs&), PKArgs& ARGS>
void ExtModule<T>::add(const char* name, const char* doc) {
  ARGS.set_class_name(static_cast<T*>(this)->name());
  ARGS.set_function_name(name);
  methods.push_back(PyMethodDef {
    name,
    reinterpret_cast<PyCFunction>(&_safe_function0<PKArgs, F, ARGS>),
    METH_VARARGS | METH_KEYWORDS,
    doc
  });
}


template <class T>
PyMethodDef* ExtModule<T>::get_methods() {
  static_cast<T*>(this)->init_methods();
  methods.push_back(PyMethodDef {nullptr, nullptr, 0, nullptr});
  return methods.data();
}


template <class T>
PyObject* ExtModule<T>::init() {
  T* self = static_cast<T*>(this);

  module_def = {
    PyModuleDef_HEAD_INIT,
    self->name(),  /* name of the module */
    self->doc(),   /* module documentation */
    -1,            /* size of per-interpreter state of the module, or -1
                      if the module keeps state in global variables */
    self->get_methods(),

    // https://docs.python.org/3/c-api/module.html#multi-phase-initialization
    nullptr,       /* m_slots */
    nullptr,       /* m_traverse */
    nullptr,       /* m_clear */
    nullptr,       /* m_free */
  };

  pymodule = PyModule_Create(&module_def);
  if (pymodule == nullptr) throw PyError();  // LCOV_EXCL_LINE
  return pymodule;
}


}

#endif
