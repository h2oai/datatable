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
#include "python/ext_type.h"
#include "extras/ftrl.h"
#include "python/namedtuple.h"

namespace py {
  class PyFtrl : public PyObject {
    private:
      Ftrl* ft;

    public:
      class Type : public ExtType<PyFtrl> {
        public:
          static PKArgs args___init__;
          static PKArgs args_fit;
          static PKArgs args_predict;
          static PKArgs args_reset;
          static const char* classname();
          static const char* classdoc();
          static bool is_subclassable() { return true; }
          static void init_methods_and_getsets(Methods&, GetSetters&);
      };

      void m__init__(PKArgs&);
      void m__dealloc__();
      void fit(const PKArgs&);
      oobj predict(const PKArgs&);
      void reset(const PKArgs&);

      // Getters and setters.
      oobj get_model() const;
      oobj get_colnames_hashes() const;
      oobj get_params() const;
      oobj get_alpha() const;
      oobj get_beta() const;
      oobj get_lambda1() const;
      oobj get_lambda2() const;
      oobj get_d() const;
      oobj get_inter() const;
      oobj get_n_epochs() const;
      void set_model(robj);
      void set_params(robj);
      void set_alpha(robj);
      void set_beta(robj);
      void set_lambda1(robj);
      void set_lambda2(robj);
      void set_d(robj);
      void set_n_epochs(robj);
      void set_inter(robj);

      // Info for a namedtuple of parameters
      static strpair params_info;
      static std::vector<strpair> params_fields_info;
      static onamedtupletype params_nttype;
  };
}
