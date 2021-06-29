//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#ifndef dt_DATATABLEMODULE_h
#define dt_DATATABLEMODULE_h
#include "python/ext_module.h"

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
    void init_methods_csv();       // csv/py_csv.cc
    void init_methods_jay();       // open_jay.cc
    void init_methods_join();      // frame/join.cc
    void init_methods_kfold();     // models/kfold.cc
    void init_methods_rbind();     // frame/rbind.cc
    void init_methods_repeat();    // frame/repeat.cc
    void init_methods_sets();      // set_funcs.cc
    void init_methods_str();       // str/py_str.cc
    void init_methods_styles();    // frame/repr/html_styles.cc
    void init_fbinary();           // expr/fbinary/pyfn.cc
    void init_fuzzy();             // utils/fuzzy.cc
    void init_funary();            // expr/funary/pyfn.cc

    #ifdef DTTEST
      void init_tests();
    #endif
};


}  // namespace py



extern "C" {

  PyMODINIT_FUNC PyInit__datatable() noexcept;

}

#endif
