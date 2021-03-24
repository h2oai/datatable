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
#ifndef dt_MODELS_LINEARMODEL_REGRESSION_h
#define dt_MODELS_LINEARMODEL_REGRESSION_h
#include "models/dt_linearmodel.h"


namespace dt {


/**
 *  Numerical regression.
 */
template <typename T /* float or double */>
class LinearModelRegression : public LinearModel<T> {
  protected:
    LinearModelFitOutput fit_model() override {
      xassert(this->dt_y_fit_->ncols() == 1);

      if (!this->is_fitted()) {
        // In the case of numeric regression there are no labels,
        // so we just use a column name for this purpose.
        const strvec& colnames = this->dt_y_fit_->get_names();
        std::unordered_map<std::string, int32_t> colnames_map = {{colnames[0], 0}};
        this->dt_labels_ = create_dt_labels_str<uint32_t>(colnames_map);
      }

      this->label_ids_fit_ = { 0 };
      this->label_ids_val_ = { 0 };

      this->col_y_fit_ = this->dt_y_fit_->get_column(0).cast(this->stype_);
      if (_notnan(this->nepochs_val_)) {
        this->col_y_val_ = this->dt_y_val_->get_column(0).cast(this->stype_);
      }

      return this->template fit_impl<T>();
    }

  public:
    /**
     *  Identity function.
     */
    T activation_fn(T x) override {
      return x;
    }

    /**
     *  Quadratic loss.
     */
    T loss_fn(T p, T y) override {
      return (p - y) * (p - y);
    }

};


} // namespace dt

#endif
