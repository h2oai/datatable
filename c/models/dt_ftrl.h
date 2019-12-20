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
#ifndef dt_MODELS_FTRL_h
#define dt_MODELS_FTRL_h
#include "models/column_hasher.h"
#include "models/utils.h"
#include "models/dt_ftrl_base.h"
#include "str/py_str.h"
#include "models/label_encode.h"


namespace dt {


/**
 *  Class template that implements all the virtual methods declared in dt::Ftrl.
 */
template <typename T /* float or double */>
class Ftrl : public dt::FtrlBase {
  private:
    // Model datatable of shape (nbins, 2 * nlabels),
    // a vector of weight pointers, and the model type.
    dtptr dt_model;
    std::vector<T*> z, n;
    FtrlModelType model_type;

    // Feature importances datatable of shape (nfeatures, 2),
    // where the first column contains feature names and the second one
    // feature importance values.
    dtptr dt_fi;

    // FTRL model parameters provided to constructor.
    FtrlParams params;

    // Individual parameters converted to T type.
    T alpha;
    T beta;
    T lambda1;
    T lambda2;

    // Vector of feature interactions.
    std::vector<intvec> interactions;

    // Helper parameters.
    T ialpha;
    T gamma;

    // Labels that are automatically extracted from the target column.
    // For binomial classification, labels are stored as
    //   index 0: negative label
    //   index 1: positive label
    // and we only train the zero model.
    dtptr dt_labels;

    // Total number of features used for training, this includes
    // dt_X->ncols columns plus their interactions.
    size_t nfeatures;

    // Vector of hashed column names.
    std::vector<uint64_t> colname_hashes;

    // Pointers to training and validation datatables, they are
    // only valid during training.
    const DataTable* dt_X_train;
    const DataTable* dt_y_train;
    const DataTable* dt_X_val;
    const DataTable* dt_y_val;

    // Other temporary parameters needed for validation.
    T nepochs_val;
    T val_error;
    size_t val_niters;

    // These mappings relate model_id's to the incoming label
    // indicators, i.e. if label_ids_train[i] == j, we train i-th model
    // on positives, when encounter j in the encoded data,
    // and train on negatives otherwise.
    std::vector<size_t> label_ids_train;
    std::vector<size_t> label_ids_val;

    // Fitting methods
    FtrlFitOutput fit_binomial();
    FtrlFitOutput fit_multinomial();
    template <typename U> FtrlFitOutput fit_regression();
    template <typename U, typename V>
    FtrlFitOutput fit(T(*)(T), U(*)(U, size_t), V(*)(V, size_t), T(*)(T, V));
    template <typename U>
    void update(const uint64ptr&, const tptr<T>&, T, U, size_t);

    template <typename F> T predict_row(const uint64ptr&, tptr<T>&, size_t, F);
    dtptr create_p(size_t);

    // Hashing methods
    std::vector<hasherptr> create_hashers(const DataTable*);
    hasherptr create_hasher(const Column&);
    void hash_row(uint64ptr&, std::vector<hasherptr>&, size_t);

    // Model helper methods
    void add_negative_class();
    void create_model();
    void adjust_model();
    void init_model();
    void init_weights();
    void reset_model_stats();

    // Do label encoding and set up mapping information
    void create_y_binomial(const DataTable*, dtptr&, std::vector<size_t>&);
    void create_y_multinomial(const DataTable*, dtptr&, std::vector<size_t>&, bool validation = false);

    // Feature importance helper methods
    void create_fi();
    void init_fi();
    void define_features();
    void normalize_rows(dtptr&);

    // Parameter helper methods.
    void init_helper_params();

  public:
    Ftrl();
    Ftrl(FtrlParams);

    // Main fitting method
    FtrlFitOutput dispatch_fit(const DataTable*, const DataTable*,
                               const DataTable*, const DataTable*,
                               double, double, size_t) override;

    // Main predicting method
    dtptr predict(const DataTable*) override;

    // Model methods
    void reset() override;
    bool is_model_trained() override;

    // Getters
    py::oobj get_model() override;
    py::oobj get_fi(bool normalize = true) override;
    FtrlModelType get_model_type() override;
    FtrlModelType get_model_type_trained() override;
    size_t get_nfeatures() override;
    size_t get_ncols() override;
    const std::vector<uint64_t>& get_colname_hashes() override;
    double get_alpha() override;
    double get_beta() override;
    double get_lambda1() override;
    double get_lambda2() override;
    uint64_t get_nbins() override;
    unsigned char get_mantissa_nbits() override;
    size_t get_nepochs() override;
    const std::vector<intvec>& get_interactions() override;
    bool get_negative_class() override;
    FtrlParams get_params() override;
    py::oobj get_labels() override;

    // Setters
    void set_model(const DataTable&) override;
    void set_fi(const DataTable&) override;
    void set_model_type(FtrlModelType) override;
    void set_model_type_trained(FtrlModelType) override;
    void set_alpha(double) override;
    void set_beta(double) override;
    void set_lambda1(double) override;
    void set_lambda2(double) override;
    void set_nbins(uint64_t) override;
    void set_mantissa_nbits(unsigned char) override;
    void set_nepochs(size_t) override;
    void set_interactions(std::vector<intvec>) override;
    void set_negative_class(bool) override;
    void set_labels(const DataTable&) override;

    // Some useful constants:
    static constexpr T T_NAN = std::numeric_limits<T>::quiet_NaN();
    static constexpr T T_EPSILON = std::numeric_limits<T>::epsilon();
};

template<class T> constexpr T Ftrl<T>::T_NAN;
template<class T> constexpr T Ftrl<T>::T_EPSILON;

extern template class Ftrl<float>;
extern template class Ftrl<double>;

} // namespace dt

#endif
