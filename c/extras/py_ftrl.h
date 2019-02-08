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
#ifndef dt_EXTRAS_PY_FTRL_h
#define dt_EXTRAS_PY_FTRL_h
#include "extras/dt_ftrl.h"

namespace py {

using dtftptr = std::unique_ptr<dt::Ftrl>;

enum class RegType : uint8_t {
  NONE        = 0,
  REGRESSION  = 1,
  BINOMIAL    = 2,
  MULTINOMIAL = 3
};


class Ftrl : public PyObject {
  private:
    std::vector<dtftptr>* dtft;
    DataTable* feature_names;
    py::olist labels;
    RegType reg_type;
    size_t : 56;

  public:
    class Type : public ExtType<Ftrl> {
      public:
        static PKArgs args___init__;
        static PKArgs args_fit;
        static PKArgs args_predict;
        static NoArgs args_reset;
        static NoArgs fn___getstate__;
        static PKArgs fn___setstate__;
        static const char* classname();
        static const char* classdoc();
        static bool is_subclassable() { return true; }
        static void init_methods_and_getsets(Methods&, GetSetters&);
    };

    // Initializers and destructor
    void m__init__(PKArgs&);
    void m__dealloc__();
    void init_dtft(dt::FtrlParams);

    // Pickling support
    oobj m__getstate__(const NoArgs&);
    void m__setstate__(const PKArgs&);

    // Learning and predicting methods
    void fit(const PKArgs&);
    void fit_binomial(DataTable*, DataTable*);
    void fit_multinomial(DataTable*, DataTable*);
    template <typename T>
    void fit_regression(DataTable*, DataTable*);
    oobj predict(const PKArgs&);
    void reset(const NoArgs&);
    void reset_feature_names();

    // Getters and setters
    oobj get_labels() const;
    oobj get_fi() const;
    oobj get_fi_tuple() const;
    oobj get_model() const;
    oobj get_colname_hashes() const;
    oobj get_params_namedtuple() const;
    oobj get_params_tuple() const;
    oobj get_alpha() const;
    oobj get_beta() const;
    oobj get_lambda1() const;
    oobj get_lambda2() const;
    oobj get_nbins() const;
    oobj get_interactions() const;
    oobj get_nepochs() const;
    void set_labels(robj);
    void set_model(robj);
    void set_params_namedtuple(robj);
    void set_params_tuple(robj);
    void set_alpha(robj);
    void set_beta(robj);
    void set_lambda1(robj);
    void set_lambda2(robj);
    void set_nbins(robj);
    void set_nepochs(robj);
    void set_interactions(robj);

    // Model validation methods
    bool has_negative_n(DataTable*) const;

    // Link functions
    static double sigmoid(double);
    static double identity(double);
    static void normalize_rows(DataTable*);

    // Helper functions
    static void normalize_fi(RealColumn<double>*);
};

} // namespace py

#endif
