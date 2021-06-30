//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_MODELS_PY_LINEARMODEL_h
#define dt_MODELS_PY_LINEARMODEL_h
#include <map>
#include "ltype.h"
#include "models/dt_linearmodel_base.h"
#include "models/py_validator.h"
#include "python/string.h"
#include "python/xobject.h"
namespace py {


/**
 *  Main class that controls Python LinearModel API.
 */
class LinearModel : public XObject<LinearModel> {
  private:
    dt::LinearModelBase* lm_;
    dt::LinearModelParams* dt_params_;
    py::onamedtuple* py_params_;

  public:
    // LinearModel API version to be used for backward compatibility
    static const size_t API_VERSION;
    static const size_t N_PARAMS;
    static void impl_init_type(XTypeMaker&);

    // Initializers and deallocator
    void m__init__(const PKArgs&);
    void m__dealloc__();
    void init_params();
    template <typename T>
    void init_dt_model(dt::LType ltype = dt::LType::MU);
    static std::map<dt::LinearModelType, std::string> create_model_type_name();

    // Pickling support
    oobj m__getstate__(const PKArgs&);
    void m__setstate__(const PKArgs&);

    // Model related methods
    oobj fit(const PKArgs&);
    oobj predict(const PKArgs&);
    void reset(const PKArgs&);
    oobj is_fitted(const PKArgs&);

    // Getters
    oobj get_labels() const;
    oobj get_model() const;
    oobj get_params_namedtuple() const;
    oobj get_params_tuple() const;
    oobj get_eta0() const;
    oobj get_eta_decay() const;
    oobj get_eta_drop_rate() const;
    oobj get_eta_schedule() const;
    oobj get_lambda1() const;
    oobj get_lambda2() const;
    oobj get_nepochs() const;
    oobj get_double_precision() const;
    oobj get_negative_class() const;
    oobj get_model_type() const;
    oobj get_seed() const;

    // Setters
    void set_model(robj);                   // Not exposed, used for unpickling only
    void set_labels(robj);                  // Not exposed, used for unpickling only
    void set_params_tuple(robj);            // Not exposed, used for unpickling only
    void set_params_namedtuple(robj);       // Not exposed, used in `m__init__` only
    void set_eta0(const Arg&);
    void set_eta_decay(const Arg&);
    void set_eta_drop_rate(const Arg&);
    void set_eta_schedule(const Arg&);
    void set_lambda1(const Arg&);
    void set_lambda2(const Arg&);
    void set_nepochs(const Arg&);
    void set_double_precision(const Arg&);  // Not exposed, used for unpickling only
    void set_negative_class(const Arg&);    // Disabled for a trained model
    void set_model_type(const Arg&);        // Disabled for a trained model
    void set_seed(const Arg&);
};


} // namespace py

#endif
