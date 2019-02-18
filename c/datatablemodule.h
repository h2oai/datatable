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
#ifndef dt_DATATABLEMODULE_h
#define dt_DATATABLEMODULE_h
#include "python/ext_module.h"


class DatatableModule : public py::ExtModule<DatatableModule> {
  public:
    const char* name() const {
      return "_datatable";
    }
    const char* doc() const {
      return "module doc...";
    }

    void init_methods();
    void init_methods_aggregate(); // extras/aggregate.cc
    void init_methods_buffers();   // py_buffers.cc
    void init_methods_csv();       // csv/py_csv.cc
    void init_methods_jay();       // open_jay.cc
    void init_methods_join();      // frame/join.cc
    void init_methods_kfold();     // extras/kfold.cc
    void init_methods_nff();       // datatable_load.cc
    void init_methods_options();   // options.cc
    void init_methods_repeat();    // frame/repeat.cc
    void init_methods_sets();      // set_funcs.cc
    void init_methods_str();       // str/py_str.cc

    #ifdef DTTEST
      void init_tests();
    #endif
};


#ifdef DTDEBUG
  void TRACK(void* ptr, size_t size, const char* name);
  void UNTRACK(void* ptr);
#else
  #define TRACK(ptr, size, name)
  #define UNTRACK(ptr)
#endif


extern SType force_stype;  // Declared in py_buffers

void init_jay();
PyMODINIT_FUNC PyInit__datatable() noexcept;


#endif
