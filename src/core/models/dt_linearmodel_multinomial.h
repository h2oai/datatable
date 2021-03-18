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
#ifndef dt_MODELS_LINEARMODEL_MULTINOMIAL_h
#define dt_MODELS_LINEARMODEL_MULTINOMIAL_h
#include "models/dt_linearmodel.h"
#include "models/utils.h"


namespace dt {


/**
 *  A class for multinomial classification.
 */
template <typename T /* float or double */>
class LinearModelMultinomial : public LinearModel<T> {
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
