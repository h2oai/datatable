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
#include <map>
#include "str/py_str.h"
#include "python/string.h"
#include "python/xobject.h"
#include "models/dt_ftrl.h"
#include "models/dt_ftrl_base.h"
#include "models/py_validator.h"
namespace py {


/**
 *  Main class that controls Python FTRL API.
 */
class Ftrl : public XObject<Ftrl> {
  private:
    dt::FtrlBase* dtft;
    py::onamedtuple* py_params;
    strvec* colnames;
    bool double_precision;
    size_t: 56;

  public:
    // FTRL API version to be used for backward compatibility
    static const size_t API_VERSION = 23;
    static void impl_init_type(XTypeMaker&);

    // Initializers and deallocator
    void m__init__(const PKArgs&);
    void m__dealloc__();
    void init_py_params();
    void init_dt_ftrl();
    void init_dt_interactions();
    static std::map<dt::FtrlModelType, std::string> create_model_type_name();

    // Pickling support
    oobj m__getstate__(const PKArgs&);
    void m__setstate__(const PKArgs&);

    // Learning and predicting methods
    oobj fit(const PKArgs&);
    oobj predict(const PKArgs&);
    void reset(const PKArgs&);

    // Getters
    oobj get_labels() const;
    oobj get_dt_labels() const;
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
    oobj get_mantissa_nbits() const;
    oobj get_nepochs() const;
    oobj get_interactions() const;
    oobj get_double_precision() const;
    oobj get_negative_class() const;
    oobj get_model_type() const;
    oobj get_model_type_trained() const;

    // Setters
    void set_model(robj);             // Not exposed, used for unpickling only
    void set_labels(robj);            // Not exposed, used for unpickling only
    void set_colnames(robj);          // Not exposed, used for unpickling only
    void set_params_tuple(robj);      // Not exposed, used for unpickling only
    void set_params_namedtuple(robj); // Not exposed, used in `m__init__` only
    void set_alpha(const Arg&);
    void set_beta(const Arg&);
    void set_lambda1(const Arg&);
    void set_lambda2(const Arg&);
    void set_nepochs(const Arg&);
    void set_nbins(const Arg&);             // Disabled for a trained model
    void set_mantissa_nbits(const Arg&);    // Disabled for a trained model
    void set_interactions(const Arg&);      // Disabled for a trained model
    void set_double_precision(const Arg&);  // Not exposed, used for unpickling only
    void set_negative_class(const Arg&);    // Disabled for a trained model
    void set_model_type(const Arg&);        // Disabled for a trained model
};


} // namespace py

#endif
