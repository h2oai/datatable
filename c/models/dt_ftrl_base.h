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
#ifndef dt_MODELS_FTRL_BASE_h
#define dt_MODELS_FTRL_BASE_h
#include "options.h"
#include "str/py_str.h"
namespace dt {


/**
 *  Supported FTRL model types.
 */
enum class FtrlModelType : size_t {
  NONE        = 0, // Untrained model
  AUTO        = 1, // Automatically detect model type
  REGRESSION  = 2, // Numerical regression
  BINOMIAL    = 3, // Binomial logistic regression
  MULTINOMIAL = 4  // Multinomial logistic regression
};


/**
 *  All the FTRL parameters provided in Python are stored in this structure,
 *  that also defines their default values.
 */
struct FtrlParams {
    FtrlModelType model_type;
    double alpha;
    double beta;
    double lambda1;
    double lambda2;
    uint64_t nbins;
    size_t nepochs;
    unsigned char mantissa_nbits;
    bool double_precision;
    bool negative_class;
    size_t : 40;
    FtrlParams() : model_type(FtrlModelType::AUTO),
                   alpha(0.005), beta(1.0), lambda1(0.0), lambda2(0.0),
                   nbins(1000000), nepochs(1), mantissa_nbits(10),
                   double_precision(false), negative_class(false)
                   {}
};


/**
 *  When FTRL fitting is completed, this structure is returned
 *  containing epoch at which fitting stopped, and, in the case validation set
 *  was provided, the corresponding final loss.
 */
struct FtrlFitOutput {
    double epoch;
    double loss;
};

using dtptr = std::unique_ptr<DataTable>;

/**
 *  An abstract dt::FtrlBase class that declares all the virtual functions
 *  needed by py::Ftrl.
 */
class FtrlBase {
  public:
    virtual ~FtrlBase();
    // Depending on the target column stype, this method should do
    // - binomial logistic regression (BOOL);
    // - multinomial logistic regression (STR32, STR64);
    // - numerical regression (INT8, INT16, INT32, INT64, FLOAT32, FLOAT64).
    virtual FtrlFitOutput dispatch_fit(const DataTable*, const DataTable*,
                                       const DataTable*, const DataTable*,
                                       double, double, size_t) = 0;
    virtual dtptr predict(const DataTable*) = 0;
    virtual void reset() = 0;
    virtual bool is_model_trained() = 0;

    // Getters
    virtual py::oobj get_model() = 0;
    virtual FtrlModelType get_model_type() = 0;
    virtual FtrlModelType get_model_type_trained() = 0;
    virtual py::oobj get_fi(bool normaliza = true) = 0;
    virtual size_t get_nfeatures() = 0;
    virtual size_t get_ncols() = 0;
    virtual const std::vector<uint64_t>& get_colname_hashes() = 0;
    virtual double get_alpha() = 0;
    virtual double get_beta() = 0;
    virtual double get_lambda1() = 0;
    virtual double get_lambda2() = 0;
    virtual uint64_t get_nbins() = 0;
    virtual unsigned char get_mantissa_nbits() = 0;
    virtual size_t get_nepochs() = 0;
    virtual const std::vector<intvec>& get_interactions() = 0;
    virtual bool get_negative_class() = 0;
    virtual FtrlParams get_params() = 0;
    virtual py::oobj get_labels() = 0;
    static size_t get_work_amount(size_t);

    // Setters
    virtual void set_model(const DataTable&) = 0;
    virtual void set_fi(const DataTable&) = 0;
    virtual void set_model_type(FtrlModelType) = 0;
    virtual void set_model_type_trained(FtrlModelType) = 0;
    virtual void set_alpha(double) = 0;
    virtual void set_beta(double) = 0;
    virtual void set_lambda1(double) = 0;
    virtual void set_lambda2(double) = 0;
    virtual void set_nbins(uint64_t) = 0;
    virtual void set_mantissa_nbits(unsigned char) = 0;
    virtual void set_nepochs(size_t) = 0;
    virtual void set_interactions(std::vector<intvec>) = 0;
    virtual void set_negative_class(bool) = 0;
    virtual void set_labels(const DataTable&) = 0;

    // Number of mantissa bits in a double number.
    static constexpr unsigned char DOUBLE_MANTISSA_NBITS = 52;

    // TODO: separator for multilabel regression.
    static constexpr char SEPARATOR = ',';

    // Minimum number of rows a thread will get for fitting and predicting.
    static constexpr size_t MIN_ROWS_PER_THREAD = 10000;
};


} // namespace dt

#endif
