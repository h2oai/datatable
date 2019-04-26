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
    PyObject* init() {
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

    void add(PyMethodDef def) {
      methods.push_back(def);
    }

    void add(PyCFunctionWithKeywords F, PKArgs& args) {
      methods.push_back(PyMethodDef {
        args.get_short_name(),
        reinterpret_cast<PyCFunction>(F),
        METH_VARARGS | METH_KEYWORDS,
        args.get_docstring()
      });
    }

  private:
    PyMethodDef* get_methods() {
      static_cast<T*>(this)->init_methods();
      methods.push_back(PyMethodDef {nullptr, nullptr, 0, nullptr});
      return methods.data();
    }

};


#define ADD_FN(FUNCTION, ARGS)                                                 \
  add(                                                                         \
    [](PyObject*, PyObject* args, PyObject* kwds) -> PyObject* {               \
      return ARGS.exec_function(args, kwds, FUNCTION);                         \
    }, ARGS);                                                                  \


} // namespace py
#endif
