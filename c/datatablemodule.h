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
#include "utils/c+++.h"

namespace py {


class DatatableModule : public ExtModule<DatatableModule> {
  public:
    const char* name() const {
      return "_datatable";
    }
    const char* doc() const {
      return "module doc...";
    }

    void init_methods();
    void init_methods_aggregate(); // models/aggregate.cc
    void init_methods_buffers();   // py_buffers.cc
    void init_methods_cbind();     // frame/cbind.cc
    void init_methods_csv();       // csv/py_csv.cc
    void init_methods_zread();
    void init_methods_isclose();   // expr/head_func_isclose.cc
    void init_methods_jay();       // open_jay.cc
    void init_methods_join();      // frame/join.cc
    void init_methods_kfold();     // models/kfold.cc
    void init_methods_rbind();     // frame/rbind.cc
    void init_methods_repeat();    // frame/repeat.cc
    void init_methods_sets();      // set_funcs.cc
    void init_methods_shift();     // expr/head_func_shift.cc
    void init_methods_str();       // str/py_str.cc
    void init_methods_styles();    // frame/repr/html_styles.cc
    void init_casts();             // frame/cast.cc
    void init_fbinary();           // expr/fbinary/pyfn.cc
    void init_fuzzy();             // utils/fuzzy.cc
    void init_funary();            // expr/funary/pyfn.cc
    void init_fnary();             // expr/fnary/pyfn.cc

    #ifdef DTTEST
      void init_tests();
    #endif
};


}  // namespace py


#ifdef DTDEBUG
  void TRACK(void* ptr, size_t size, const char* name);
  void UNTRACK(void* ptr);
  bool IS_TRACKED(void* ptr);
#else
  #define TRACK(ptr, size, name)
  #define UNTRACK(ptr)
  #define IS_TRACKED(ptr) 1
#endif


namespace pybuffers {
  extern size_t single_col;  // Declared in py_buffers
  extern SType force_stype;
}


const char* get_compiler_version_string();

extern "C" {

  PyMODINIT_FUNC PyInit__datatable() noexcept;

}

#endif
