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
#ifndef dt_MODELS_PY_FTRL_h
#define dt_MODELS_PY_FTRL_h
#include "str/py_str.h"
#include "python/string.h"
#include "models/dt_ftrl.h"
#include "models/dt_ftrl_real.h"
#include "models/py_validator.h"

namespace py {


/*
* Main class that controls Python FTRL API.
*/
class Ftrl : public PyObject {
  private:
    dt::Ftrl* dtft;
    py::oobj py_interactions;
    strvec colnames;

  public:
    class Type : public ExtType<Ftrl> {
      public:
        static PKArgs args___init__;
        static const char* classname();
        static const char* classdoc();
        static bool is_subclassable() { return true; }
        static void init_methods_and_getsets(Methods&, GetSetters&);
    };

    // Initializers and deallocator
    void m__init__(PKArgs&);
    void m__dealloc__();

    // Pickling support
    oobj m__getstate__(const PKArgs&);
    void m__setstate__(const PKArgs&);

    // Learning and predicting methods
    oobj fit(const PKArgs&);
    oobj predict(const PKArgs&);
    void reset(const PKArgs&);
    std::vector<sizetvec> convert_interactions();

    // Getters
    oobj get_labels() const;
    oobj get_fi() const;
    oobj get_normalized_fi(bool) const;
    oobj get_model() const;
    oobj get_colnames() const;
    oobj get_colname_hashes() const;
    oobj get_params_namedtuple() const;
    oobj get_params_tuple() const;
    oobj get_alpha() const;
    oobj get_beta() const;
    oobj get_lambda1() const;
    oobj get_lambda2() const;
    oobj get_nbins() const;
    oobj get_nepochs() const;
    oobj get_interactions() const;
    oobj get_double_precision() const;
    oobj get_negative_class() const;

    // Setters
    void set_model(robj);             // Not exposed, used for unpickling only
    void set_labels(robj);            // Not exposed, used for unpickling only
    void set_colnames(robj);          // Not exposed, used for unpickling only
    void set_params_tuple(robj);      // Not exposed, used for unpickling only
    void set_alpha(robj);
    void set_beta(robj);
    void set_lambda1(robj);
    void set_lambda2(robj);
    void set_nepochs(robj);
    void set_nbins(robj);             // Disabled for a trained model
    void set_interactions(robj);      // Disabled for a trained model
    void set_double_precision(robj);  // Not exposed, used for unpickling only
    void set_negative_class(robj);    // Disabled for a trained model
};


} // namespace py

#endif
