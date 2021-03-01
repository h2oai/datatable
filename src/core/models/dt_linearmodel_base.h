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
#ifndef dt_MODELS_LINEARMODEL_BASE_h
#define dt_MODELS_LINEARMODEL_BASE_h
#include "options.h"
#include "str/py_str.h"
namespace dt {


/**
 *  Supported linear model types.
 */
enum class LinearModelType : size_t {
  NONE        = 0, // Untrained model
  AUTO        = 1, // Automatically detect model type
  REGRESSION  = 2, // Numerical regression
  BINOMIAL    = 3, // Binomial logistic regression
  MULTINOMIAL = 4  // Multinomial logistic regression
};


/**
 *  All the LinearModel parameters provided in Python are stored in this structure,
 *  that also defines their default values.
 */
struct LinearModelParams {
  LinearModelType model_type;
  double eta;
  double lambda1;
  double lambda2;
  uint64_t nbins;
  double nepochs;
  bool double_precision;
  bool negative_class;
  size_t : 48;
  LinearModelParams() : model_type(LinearModelType::AUTO),
                 eta(0.005), lambda1(0.0), lambda2(0.0),
                 nbins(1000000), nepochs(1.0), double_precision(false),
                 negative_class(false)
                 {}
};


/**
 *  When linear model fitting is completed, this structure is returned
 *  containing epoch at which fitting stopped, and, in the case validation set
 *  was provided, the corresponding final loss.
 */
struct LinearModelFitOutput {
  double epoch;
  double loss;
  LinearModelFitOutput() {
    epoch = std::numeric_limits<double>::quiet_NaN();
    loss = std::numeric_limits<double>::quiet_NaN();
  }
  LinearModelFitOutput(double epoch_, double loss_) {
    epoch = epoch_;
    loss = loss_;
  }
};


/**
 *  An abstract dt::LinearBase class that declares all the virtual functions
 *  needed by py::Linear.
 */
class LinearModelBase {
  public:
    virtual ~LinearModelBase();
    // Depending on the target column stype, this method should do
    // - binomial logistic regression (BOOL);
    // - multinomial logistic regression (STR32, STR64);
    // - numerical regression (INT8, INT16, INT32, INT64, FLOAT32, FLOAT64).
    virtual LinearModelFitOutput dispatch_fit(const DataTable*, const DataTable*,
                                       const DataTable*, const DataTable*,
                                       double, double, size_t) = 0;
    virtual dtptr predict(const DataTable*) = 0;
    virtual void reset() = 0;
    virtual bool is_model_trained() = 0;

    // Getters
    virtual py::oobj get_model() = 0;
    virtual LinearModelType get_model_type() = 0;
    virtual LinearModelType get_model_type_trained() = 0;
    virtual py::oobj get_fi(bool normaliza = true) = 0;
    virtual size_t get_nfeatures() = 0;
    virtual size_t get_nlabels() = 0;
    virtual size_t get_ncols() = 0;
    virtual double get_eta() = 0;
    virtual double get_lambda1() = 0;
    virtual double get_lambda2() = 0;
    virtual double get_nepochs() = 0;
    virtual bool get_negative_class() = 0;
    virtual LinearModelParams get_params() = 0;
    virtual py::oobj get_labels() = 0;
    static size_t get_work_amount(size_t);

    // Setters
    virtual void set_model(const DataTable&) = 0;
    virtual void set_fi(const DataTable&) = 0;
    virtual void set_model_type(LinearModelType) = 0;
    virtual void set_model_type_trained(LinearModelType) = 0;
    virtual void set_eta(double) = 0;
    virtual void set_lambda1(double) = 0;
    virtual void set_lambda2(double) = 0;
    virtual void set_nepochs(double) = 0;
    virtual void set_negative_class(bool) = 0;
    virtual void set_labels(const DataTable&) = 0;

    // TODO: separator for multilabel regression.
    static constexpr char SEPARATOR = ',';

    // Minimum number of rows a thread will get for fitting and predicting.
    static constexpr size_t MIN_ROWS_PER_THREAD = 10000;
};


} // namespace dt

#endif
