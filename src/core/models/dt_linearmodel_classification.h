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
#ifndef dt_MODELS_LINEARMODEL_CLASSIFICATION_h
#define dt_MODELS_LINEARMODEL_CLASSIFICATION_h
#include "models/dt_linearmodel.h"


namespace dt {


/**
 *  Base class for all the classification models.
 */
template <typename T /* float or double */>
class LinearModelClassification : public LinearModel<T> {
  public:
    // Sigmoid
    T activation_fn(T x) override {
      return T(1) / (T(1) + std::exp(-x));
    }
    // Logloss
    T loss_fn(T p, T y) override {
      constexpr T epsilon = std::numeric_limits<T>::epsilon();
      p = std::max(std::min(p, 1 - epsilon), epsilon);
      return -std::log(p * (2*y - 1) + 1 - y);
    }
};


/**
 *  Binomial classification. Note, we only train the positive class
 *  in this case.
 */
template <typename T /* float or double */>
class LinearModelBinomial : public LinearModelClassification<T> {
  protected:
    LinearModelFitOutput fit_model() override {
      dtptr dt_y_fit, dt_y_val;
      bool validation = _notnan(this->nepochs_val_);
      create_y_binomial(this->dt_y_fit_, dt_y_fit,
                        this->label_ids_fit_, this->dt_labels_);

      // NA values are ignored during training, so we stop training right away,
      // if got NA's only.
      if (dt_y_fit == nullptr) {
        return {0, static_cast<double>(this->T_NAN)};
      }
      this->col_y_fit_ = dt_y_fit.get()->get_column(0);

      if (validation) {
        create_y_binomial(this->dt_y_val_, dt_y_val,
                          this->label_ids_val_, this->dt_labels_);
        if (dt_y_val == nullptr) {
          throw ValueError() << "Cannot set early stopping criteria as validation "
                                "target column got `NA` targets only";
        }
        this->col_y_val_ = dt_y_val.get()->get_column(0);
      }

      return this->template fit_impl<int8_t>();
    }

    // Calculate predictions for the negative class
    void finalize_predict(std::vector<T*> data_p,
                          const size_t nrows,
                          const int32_t* data_label_ids) override
    {
      if (data_p.size() == 2) {
        size_t positive_class_id = (data_label_ids[0] == 1);

        // This ensures predictions sum up to one
        dt::parallel_for_static(nrows, [&](size_t i){
          data_p[!positive_class_id][i] = T(1) - data_p[positive_class_id][i];
        });
      }
    }


  public:
    size_t get_nclasses() override {
      return 1;
    }

    // This method will also adjust `k` to the positive label id, if needed
    size_t get_label_id(size_t& k, const int32_t* data_label_ids) override {
      xassert(k ==0 || k == 1);
      if (data_label_ids[k] == 1) {
        k = 1;
      }
      return 0;
    }

};


/**
 *  Multinomial classification.
 */
template <typename T /* float or double */>
class LinearModelMultinomial : public LinearModelClassification<T> {
  protected:
    LinearModelFitOutput fit_model() override {
      dtptr dt_y_fit;
      size_t n_new_labels = create_y_multinomial(
                              this->dt_y_fit_, dt_y_fit,
                              this->label_ids_fit_, this->dt_labels_,
                              this->negative_class_
                            );

      // Adjust trained model if we got new labels
      if (n_new_labels) {
        xassert(this->is_fitted());
        this->adjust_model();
      }

      if (dt_y_fit == nullptr) {
        return {0, static_cast<double>(this->T_NAN)};
      }
      this->col_y_fit_ = dt_y_fit.get()->get_column(0);

      // Create validation targets if needed.
      dtptr dt_y_val;
      if (_notnan(this->nepochs_val_)) {
        create_y_multinomial(this->dt_y_val_, dt_y_val,
                             this->label_ids_val_, this->dt_labels_,
                             this->negative_class_, true);
        if (dt_y_val == nullptr)
          throw ValueError() << "Cannot set early stopping criteria as validation "
                             << "target column got `NA` targets only";
        this->col_y_val_ = dt_y_val.get()->get_column(0);
      }

      return this->template fit_impl<int32_t>();
    }

    void finalize_predict(std::vector<T*> data_p,
                          const size_t nrows,
                          const int32_t*) override
    {
      if (data_p.size() > 2) softmax(data_p, nrows);
    }
};



} // namespace dt

#endif
