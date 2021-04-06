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

#include <limits>                        // std::numeric_limits
#include "models/dt_linearmodel_base.h"
#include "models/label_encode.h"
#include "models/utils.h"


namespace dt {

/**
 *  An abstract class that implements all the virtual methods declared in
 *  `dt::LinearModelBase`. It also declares problem-specific virtual methods,
 *  such as `fit_model()`, `activation_fn()`, `loss_fn()`, etc.
 */
template <typename T /* float or double */>
class LinearModel : public LinearModelBase {
  protected:
    // Model coefficients
    dtptr dt_model_;
    std::vector<T*> betas_;

    // Feature importances datatable of shape (nfeatures, 2),
    // where the first column contains feature names and the second one
    // feature importance values.
    dtptr dt_fi_;

    // Individual parameters converted to T type.
    T eta0_;
    T eta_decay_;
    T eta_drop_rate_;
    T lambda1_;
    T lambda2_;
    T nepochs_;
    unsigned int seed_;
    bool negative_class_;

    // SType that corresponds to `T`
    SType stype_;
    size_t : 16;

    LearningRateSchedule eta_schedule_;

    // Labels that are automatically extracted from the target column.
    // For binomial classification, labels are stored as
    //   index 0: negative label
    //   index 1: positive label
    // and we only train the zeroth model.
    dtptr dt_labels_;

    // Total number of features used for training, this should always
    // be equal to `dt_X_->ncols()`
    size_t nfeatures_;

    // Pointers to training and validation datatables, they are
    // only valid during training.
    const DataTable* dt_X_fit_;
    const DataTable* dt_y_fit_;
    const DataTable* dt_X_val_;
    const DataTable* dt_y_val_;
    Column col_y_fit_, col_y_val_;

    // Other temporary parameters needed for validation.
    T nepochs_val_;
    T val_error_;
    size_t val_niters_;

    // These mappings relate model_id's to the incoming label
    // indicators, i.e. if label_ids_train[i] == j, we train i-th model
    // on positives, when encounter j in the encoded data,
    // and train on negatives otherwise.
    std::vector<size_t> label_ids_fit_;
    std::vector<size_t> label_ids_val_;

    virtual LinearModelFitOutput fit_model() = 0;
    template <typename U>
    LinearModelFitOutput fit_impl();
    T predict_row(const tptr<T>& x, const std::vector<T*>, const size_t);
    dtptr create_p(size_t);
    virtual void finalize_predict(std::vector<T*>, const size_t, const int32_t*) {}
    void adjust_eta(T&, size_t);

    // Model helper methods
    void create_model();
    void adjust_model();
    void init_model();
    std::vector<T*> get_model_data(const dtptr&);

  public:
    LinearModel();

    // Fitting and predicting
    LinearModelFitOutput fit(
      const LinearModelParams*,           // Training parameters
      const DataTable*, const DataTable*, // Training frames
      const DataTable*, const DataTable*, // Validation frames
      double, double, size_t              // Validation parameters
    ) override;

    dtptr predict(const DataTable*) override;
    bool is_fitted() override;
    bool read_row(const size_t, const colvec&, tptr<T>&);

    virtual T activation_fn(T) = 0;
    virtual T loss_fn(T, T) = 0;

    template <typename U>
    T target_fn(U, size_t);               // Classification
    T target_fn(T, size_t);               // Regression


    // Getters
    py::oobj get_model() override;
    size_t get_nfeatures() override;
    size_t get_nlabels() override;
    bool get_negative_class();
    py::oobj get_labels() override;
    virtual size_t get_label_id(size_t&, const int32_t*);
    virtual size_t get_nclasses();

    // Setters
    void set_model(const DataTable&) override;
    void set_labels(const DataTable&) override;

    static constexpr T T_NAN = std::numeric_limits<T>::quiet_NaN();
    static constexpr T T_EPSILON = std::numeric_limits<T>::epsilon();
};

template<class T> constexpr T LinearModel<T>::T_NAN;
template<class T> constexpr T LinearModel<T>::T_EPSILON;

extern template class LinearModel<float>;
extern template class LinearModel<double>;

extern template LinearModelFitOutput LinearModel<float>::fit_impl<int8_t>();
extern template LinearModelFitOutput LinearModel<float>::fit_impl<int32_t>();
extern template LinearModelFitOutput LinearModel<float>::fit_impl<float>();
extern template LinearModelFitOutput LinearModel<double>::fit_impl<int8_t>();
extern template LinearModelFitOutput LinearModel<double>::fit_impl<int32_t>();
extern template LinearModelFitOutput LinearModel<double>::fit_impl<double>();


} // namespace dt

#endif
