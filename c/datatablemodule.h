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
    void init_methods_aggregate();// extra/aggergate.cc
    void init_methods_ftrl();     // extra/ftrl.cc
    void init_methods_str();      // str/py_str.cc
    void init_methods_options();  // options.cc
    void init_methods_sets();     // set_funcs.cc
};


#endif
