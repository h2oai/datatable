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
#ifndef dt_MODELS_LINEARMODEL_h
#define dt_MODELS_LINEARMODEL_h
#include "models/utils.h"
#include "models/dt_linearmodel_base.h"
#include "str/py_str.h"
#include "models/label_encode.h"


namespace dt {


/**
 *  Class template that implements all the virtual methods declared in
 *  dt::LinearModelBase.
 */
template <typename T /* float or double */>
class LinearModel : public LinearModelBase {
  private:
    // SType that corresponds to `T`
    const SType stype;
    size_t : 56;
    // Model's shape is (nfeatures + 1, nlabels)
    dtptr dt_model;
    std::vector<T*> w;
    LinearModelType model_type;

    // Feature importances datatable of shape (nfeatures, 2),
    // where the first column contains feature names and the second one
    // feature importance values.
    dtptr dt_fi;

    // Linear model parameters provided to constructor.
    LinearModelParams params;

    // Individual parameters converted to T type.
    T eta;
    T lambda1;
    T lambda2;
    T nepochs;

    // Vector of feature interactions.
    std::vector<sztvec> interactions;

    // Labels that are automatically extracted from the target column.
    // For binomial classification, labels are stored as
    //   index 0: negative label
    //   index 1: positive label
    // and we only train the zero model.
    dtptr dt_labels;

    // Total number of features used for training, this includes
    // dt_X->ncols columns plus their interactions.
    size_t nfeatures;

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
    LinearModelFitOutput fit_binomial();
    LinearModelFitOutput fit_multinomial();
    template <typename U> LinearModelFitOutput fit_regression();
    template <typename U, typename V>
    LinearModelFitOutput fit(T(*)(T), T(*)(T), U(*)(U, size_t), V(*)(V, size_t), T(*)(T, T));

    template <typename F> T predict_row(const tptr<T>& x, const size_t, F);
    dtptr create_p(size_t);

    // Model helper methods
    void add_negative_class();
    void create_model();
    void adjust_model();
    void init_model();
    void init_weights();

    // Feature importance helper methods
    void create_fi();
    void init_fi();
    void define_features();
    void softmax_rows(std::vector<T*>&, const size_t);


  public:
    LinearModel();
    LinearModel(LinearModelParams);

    // Main fitting method
    LinearModelFitOutput dispatch_fit(const DataTable*, const DataTable*,
                               const DataTable*, const DataTable*,
                               double, double, size_t) override;

    // Main predicting method
    dtptr predict(const DataTable*) override;

    // Model methods
    void reset() override;
    bool is_model_trained() override;

    // Helpers
    bool read_row(const size_t, const colvec&, tptr<T>&);

    // Getters
    py::oobj get_model() override;
    py::oobj get_fi(bool normalize = true) override;
    LinearModelType get_model_type() override;
    LinearModelType get_model_type_trained() override;
    size_t get_nfeatures() override;
    size_t get_nlabels() override;
    size_t get_ncols() override;
    double get_eta() override;
    double get_lambda1() override;
    double get_lambda2() override;
    double get_nepochs() override;
    const std::vector<sztvec>& get_interactions() override;
    bool get_negative_class() override;
    LinearModelParams get_params() override;
    py::oobj get_labels() override;

    // Setters
    void set_model(const DataTable&) override;
    void set_fi(const DataTable&) override;
    void set_model_type(LinearModelType) override;
    void set_model_type_trained(LinearModelType) override;
    void set_eta(double) override;
    void set_lambda1(double) override;
    void set_lambda2(double) override;
    void set_nepochs(double) override;
    void set_interactions(std::vector<sztvec>) override;
    void set_negative_class(bool) override;
    void set_labels(const DataTable&) override;

    // Some useful constants:
    static constexpr T T_NAN = std::numeric_limits<T>::quiet_NaN();
    static constexpr T T_EPSILON = std::numeric_limits<T>::epsilon();
};

template<class T> constexpr T LinearModel<T>::T_NAN;
template<class T> constexpr T LinearModel<T>::T_EPSILON;

extern template class LinearModel<float>;
extern template class LinearModel<double>;

} // namespace dt

#endif
