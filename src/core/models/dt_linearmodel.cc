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
#include <algorithm>            // std::max
#include <cmath>                // std::ceil
#include "column.h"
#include "frame/py_frame.h"
#include "models/column_caster.h"
#include "models/dt_linearmodel.h"
#include "parallel/api.h"
#include "parallel/atomic.h"
#include "progress/work.h"      // dt::progress::work
#include "utils/macros.h"


namespace dt {


/**
 *  Constructor
 */
template <typename T>
LinearModel<T>::LinearModel() {
  stype_ = sizeof(T) == 4? dt::SType::FLOAT32 : dt::SType::FLOAT64;
}


/**
 *  This method calls `fit_model()`, that a derived class
 *  should implement.
 */
template <typename T>
LinearModelFitOutput LinearModel<T>::fit(
  const LinearModelParams* params,
  const DataTable* dt_X_fit, const DataTable* dt_y_fit, const DataTable* dt_X_val,
  const DataTable* dt_y_val,
  double nepochs_val,
  double val_error,
  size_t val_niters
)
{
  // Cast input parameters to `T`
  eta_ = static_cast<T>(params->eta);
  lambda1_ = static_cast<T>(params->lambda1);
  lambda2_ = static_cast<T>(params->lambda2);
  nepochs_ = static_cast<T>(params->nepochs);
  negative_class_ = params->negative_class;

  dt_X_fit_ = dt_X_fit; dt_y_fit_ = dt_y_fit; dt_X_val_ = dt_X_val;
  dt_y_val_ = dt_y_val;
  nepochs_val_ = static_cast<T>(nepochs_val);
  val_error_ = static_cast<T>(val_error);
  val_niters_ = val_niters;
  label_ids_fit_.clear(); label_ids_val_.clear();

  auto res = fit_model();

  dt_X_fit_ = nullptr; dt_y_fit_ = nullptr; dt_X_val_ = nullptr;
  dt_y_val_ = nullptr;
  nepochs_val_ = T_NAN;
  val_error_ = T_NAN;
  return res;
}



/**
 *  Fit a model by using ordinary least squares formulation with
 *  stohastic gradient descent learning and elastic net regularization,
 *  see these references for more details:
 *  - https://en.wikipedia.org/wiki/Stochastic_gradient_descent
 *  - https://en.wikipedia.org/wiki/Elastic_net_regularization
 */
template <typename T>
template <typename U> /* target column(s) data type */
LinearModelFitOutput LinearModel<T>::fit_impl() {
  // Initialization
  if (!is_fitted()) {
    nfeatures_ = dt_X_fit_->ncols(); create_model();
  }
  init_coefficients();
  if (dt_fi_ == nullptr) create_fi();
  colvec cols = make_casted_columns(dt_X_fit_, stype_);
  // Obtain rowindex and data pointers for the target column(s).
  auto data_fi = static_cast<T*>(dt_fi_->get_column(1).get_data_editable());

  // Since `nepochs` can be a float value
  // - the model is trained `niterations - 1` times on
  //   `iteration_nrows` rows, where `iteration_nrows == dt_X_fit_->nrows()`;
  // - then, the model is trained on the remaining `last_iteration_nrows` rows,
  //   where `0 < last_iteration_nrows <= dt_X_fit_->nrows()`.
  // If `nepochs_` is an integer number, `last_iteration_nrows == dt_X_fit_->nrows()`,
  // so that the last epoch becomes identical to all the others.
  size_t niterations = static_cast<size_t>(std::ceil(nepochs_));
  T last_epoch = nepochs_ - static_cast<T>(niterations) + 1;

  size_t iteration_nrows = dt_X_fit_->nrows();
  size_t last_iteration_nrows = static_cast<size_t>(last_epoch * static_cast<T>(iteration_nrows));
  size_t total_nrows = (niterations - 1) * iteration_nrows + last_iteration_nrows;
  size_t iteration_end;

  // If a validation set is provided, we adjust the `iteration_nrows`
  // to correspond to the `nepochs_val_` epochs. After each iteration, we calculate
  // the loss on the validation dataset, and trigger early stopping
  // if relative loss does not decrese by at least `val_error_`.
  bool validation = _notnan(nepochs_val_);
  T loss = T_NAN;    // This value is returned when validation is not enabled
  T loss_old = T(0); // Value of `loss` for a previous iteraction
  std::vector<T> loss_history;
  colvec cols_val;

  if (validation) {
    cols_val = make_casted_columns(dt_X_val_, stype_);
    iteration_nrows = static_cast<size_t>(std::ceil(nepochs_val_ * static_cast<T>(iteration_nrows)));
    niterations = total_nrows / iteration_nrows + (total_nrows % iteration_nrows > 0);
    loss_history.resize(val_niters_, 0.0);
  }


  // Mutex to update global feature importance information
  std::mutex m;

  // Calculate work amounts for full fit iterations, last fit iteration and
  // validation.
  size_t work_total = (niterations - 1) * get_work_amount(iteration_nrows, MIN_ROWS_PER_THREAD);
  work_total += get_work_amount(total_nrows - (niterations - 1) * iteration_nrows, MIN_ROWS_PER_THREAD);
  if (validation) work_total += niterations * get_work_amount(dt_X_val_->nrows(), MIN_ROWS_PER_THREAD);

  // Set work amount to be reported by the zero thread.
  dt::progress::work job(work_total);
  job.set_message("Fitting...");
  NThreads nthreads = nthreads_from_niters(iteration_nrows, MIN_ROWS_PER_THREAD);

  dt::parallel_region(nthreads,
    [&]() {
      // Each thread gets a private storage observations and feature importances.
      tptr<T> x = tptr<T>(new T[nfeatures_]);
      tptr<T> fi = tptr<T>(new T[nfeatures_]());

      for (size_t iter = 0; iter < niterations; ++iter) {
        size_t iteration_start = iter * iteration_nrows;
        iteration_end = (iter == niterations - 1)? total_nrows :
                                                   (iter + 1) * iteration_nrows;
        size_t iteration_size = iteration_end - iteration_start;

        // Training.
        dt::nested_for_static(iteration_size, ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
          size_t ii = (iteration_start + i) % dt_X_fit_->nrows(); U target;
          bool isvalid = col_y_fit_.get_element(ii, &target);
          if (isvalid && _isfinite(target) && read_row(ii, cols, x)) {
            // Loop over all the labels
            for (size_t k = 0; k < label_ids_fit_.size(); ++k) {
              T p = activation_fn(predict_row(x, k,
                      [&](size_t f_id, T f_imp) {
                        fi[f_id] += f_imp;
                      }
                    ));
              T y = target_fn(target, label_ids_fit_[k]);
              T grad = (p - y);                           // Same gradient for both regression and classification
              // Update all the coefficients with SGD
              for (size_t j = 0; j < nfeatures_ + 1; ++j) {
                if (j) grad *= x[j - 1];
                grad += copysign(lambda1_, betas_[k][j]); // L1 regularization
                grad += 2 * lambda2_ * betas_[k][j];      // L2 regularization

                // Update coefficients only when the final `grad` is finite
                if (_isfinite(grad)) {
                  betas_[k][j] -= eta_ * grad;
                }
              }
            }
          }

          // Report progress
          if (dt::this_thread_index() == 0) {
            job.add_done_amount(1);
          }

        }); // End training.
        barrier();

        // Validation and early stopping.
        if (validation) {
          dt::atomic<T> loss_global {0.0};
          T loss_local = 0.0;

          dt::nested_for_static(dt_X_val_->nrows(), ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
            U y_val;
            bool isvalid = col_y_val_.get_element(i, &y_val);

            if (isvalid && _isfinite(y_val) && read_row(i, cols_val, x)) {
              for (size_t k = 0; k < label_ids_val_.size(); ++k) {
                T p = activation_fn(predict_row(
                        x, k, [&](size_t, T){}
                      ));
                T y = target_fn(y_val, label_ids_val_[k]);
                loss_local += loss_fn(p, y);
              }
            }

            // Report progress
            if (dt::this_thread_index() == 0) {
              job.add_done_amount(1);
            }

          });

          loss_global.fetch_add(loss_local);
          barrier();

          // Thread #0 checks relative loss change and, if it does not decrease
          // more than `val_error_`, sets `loss_old` to `NaN` -> this will stop
          // all the threads after `barrier()`.
          if (dt::this_thread_index() == 0) {
            T loss_current = loss_global.load() / static_cast<T>(dt_X_val_->nrows() * dt_y_val_->ncols());
            loss_history[iter % val_niters_] = loss_current / static_cast<T>(val_niters_);
            loss = std::accumulate(loss_history.begin(), loss_history.end(), T(0));
            T loss_diff = (loss_old - loss) / loss_old;
            bool is_loss_bad = (iter >= val_niters_) && (loss < T_EPSILON || loss_diff < val_error_);
            loss_old = is_loss_bad? T_NAN : loss;
          }
          barrier();

          double epoch = static_cast<double>(iteration_end) / static_cast<double>(dt_X_fit_->nrows()); if (std::isnan(loss_old)) {
            if (dt::this_thread_index() == 0) {
              job.set_message("Stopping at epoch " + tostr(epoch) +
                              ", loss = " + tostr(loss));
              // In some cases this makes progress "jumping" to 100%.
              job.set_done_amount(work_total);
            }
            break;
          }
          if (dt::this_thread_index() == 0) {
            job.set_message("Fitting... epoch " + tostr(epoch) +
                            " of " + tostr(nepochs_) + ", loss = " + tostr(loss));
          }
        } // End validation

        // Update global feature importances with the local data.
        std::lock_guard<std::mutex> lock(m);
        for (size_t i = 0; i < nfeatures_; ++i) {
          data_fi[i] += fi[i];
        }

      } // End iteration.

    }
  );
  job.done();

  double epoch_stopped = static_cast<double>(iteration_end) / static_cast<double>(dt_X_fit_->nrows()); LinearModelFitOutput res = {epoch_stopped, static_cast<double>(loss)};

  return res;
}


template <typename T>
bool LinearModel<T>::read_row(const size_t row, const colvec& cols, tptr<T>& x) {
  bool isvalid = true;

  // Read in feature values.
  for (size_t i = 0; i < cols.size(); ++i) {
    isvalid = cols[i].get_element(row, &x[i]);
    if (!isvalid) break;
  }

  return isvalid;
}


/**
 *  Predict on one row and update feature importances if requested.
 */
template <typename T>
template <typename F>
T LinearModel<T>::predict_row(const tptr<T>& x, const size_t k, F fifn) {
  T wTx = betas_[k][0];
  for (size_t i = 0; i < nfeatures_; ++i) {
    wTx += betas_[k][i + 1] * x[i];
    fifn(i, abs(betas_[k][i + 1] * x[i]));
  }
  return wTx;
}


/**
 *  Predict on a dataset.
 */
template <typename T>
dtptr LinearModel<T>::predict(const DataTable* dt_X) {
  xassert(is_fitted());

  // Re-acquire model weight pointers.
  init_coefficients();
  colvec cols = make_casted_columns(dt_X, stype_);

  // Create datatable for predictions and obtain column data pointers.
  size_t nlabels = dt_labels_->nrows();

  auto data_label_ids = static_cast<const int32_t*>(
                          dt_labels_->get_column(1).get_data_readonly()
                        );

  dtptr dt_p = create_p(dt_X->nrows());
  std::vector<T*> data_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    data_p[i] = static_cast<T*>(dt_p->get_column(i).get_data_editable());
  }


  NThreads nthreads = nthreads_from_niters(dt_X->nrows(), MIN_ROWS_PER_THREAD);

  // Set progress reporting
  size_t work_total = get_work_amount(dt_X->nrows(), MIN_ROWS_PER_THREAD);

  dt::progress::work job(work_total);
  job.set_message("Predicting...");

  dt::parallel_region(NThreads(nthreads), [&]() {
    tptr<T> x = tptr<T>(new T[nfeatures_]);

    dt::nested_for_static(dt_X->nrows(), ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
      // Predicting for all the fitted classes
      if (read_row(i, cols, x)) {
        for (size_t k = 0; k < get_nclasses(); ++k) {
          size_t label_id = get_label_id(k, data_label_ids);
          data_p[k][i] = activation_fn(predict_row(x, label_id, [&](size_t, T){}));
        }
      }

      // Progress reporting
      if (dt::this_thread_index() == 0) {
        job.add_done_amount(1);
      }
    });
  });
  job.done();

  // Here we do the following:
  // - for binomial classification we calculate predictions for the negative
  //   class as 1 - p, where p is the positive class predictions calculated above;
  // - for multinomial classification we do softmax normalization of the
  //   calculated predictions;
  // - for regression this function is a noop.
  finalize_predict(data_p, dt_X->nrows(), data_label_ids);

  return dt_p;
}


/**
 *  Get label id for a k-th trained class. Binomial classifier
 *  should override this method.
 */
template <typename T>
size_t LinearModel<T>::get_label_id(size_t& k, const int32_t* data_label_ids) {
  return static_cast<size_t>(data_label_ids[k]);
}


/**
 *  Create a model and initialize its coefficients.
 */
template <typename T>
void LinearModel<T>::create_model() {
  size_t ncols = get_nclasses();

  colvec cols;
  cols.reserve(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    cols.push_back(Column::new_data_column(nfeatures_ + 1, stype_));
  }
  dt_model_ = dtptr(new DataTable(std::move(cols), DataTable::default_names));
  init_model();
}


/**
 *  The number of classes the model is built for. Binomial classifier
 *  should override this method.
 */
template <typename T>
size_t LinearModel<T>::get_nclasses() {
  return (dt_labels_ == nullptr)? 0 : dt_labels_->nrows();
}


/**
 *  This method is invoked in the case when we get new labels
 *  for multinomial classification and need to add them to the model.
 *  In such a case, we make a copy of the "negative" coefficients
 *  adding them to the existing `dt_model_` columns.
 */
template <typename T>
void LinearModel<T>::adjust_model() {
  size_t ncols_model = dt_model_->ncols();
  size_t ncols_model_new = dt_labels_->nrows();
  xassert(ncols_model_new > ncols_model);

  colvec cols;
  cols.reserve(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols.push_back(dt_model_->get_column(i));
  }

  Column wcol;
  // If `negative_class_` parameter is set to `True`, all the new classes
  // get a copy of `w` coefficients of the `_negative_class_`.
  // Otherwise, new classes start learning from the zero coefficients.
  if (negative_class_) {
    wcol = dt_model_->get_column(0);
  } else {
    Column col = Column::new_data_column(nfeatures_ + 1, stype_);
    auto data = static_cast<T*>(col.get_data_editable());
    std::memset(data, 0, (nfeatures_ + 1) * sizeof(T));
    wcol = col;
  }

  for (size_t i = ncols_model; i < ncols_model_new; ++i) {
    cols.push_back(wcol);
  }

  dt_model_ = dtptr(new DataTable(std::move(cols), DataTable::default_names));
}


/**
 *  Create datatable for predictions.
 */
template <typename T>
dtptr LinearModel<T>::create_p(size_t nrows) {
  size_t nlabels = dt_labels_->nrows();
  xassert(nlabels > 0);

  Column col0_str64 = dt_labels_->get_column(0).cast(SType::STR64);

  strvec labels_vec(nlabels);

  for (size_t i = 0; i < nlabels; ++i) {
    CString val;
    bool isvalid = col0_str64.get_element(i, &val);
    labels_vec[i] = isvalid? val.to_string() : std::string();
  }

  colvec cols;
  cols.reserve(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    cols.push_back(Column::new_data_column(nrows, stype_));
  }

  dtptr dt_p = dtptr(new DataTable(std::move(cols), std::move(labels_vec)));
  return dt_p;
}


/**
 *  Initialize model coefficients with zeros.
 */
template <typename T>
void LinearModel<T>::init_model() {
  if (dt_model_ == nullptr) return;
  xassert(dt_model_->nrows() == nfeatures_ + 1);
  for (size_t i = 0; i < dt_model_->ncols(); ++i) {
    auto data = static_cast<T*>(dt_model_->get_column(i).get_data_editable());
    std::memset(data, 0, (nfeatures_ + 1) * sizeof(T));
  }
}


/**
 *  Obtain pointers to the model column data.
 */
template <typename T>
void LinearModel<T>::init_coefficients() {
  size_t nlabels = dt_model_->ncols();
  betas_.clear();
  betas_.reserve(nlabels);

  for (size_t k = 0; k < nlabels; ++k) {
    betas_.push_back(static_cast<T*>(dt_model_->get_column(k).get_data_editable()));
  }
}


/**
 * Create feature importance datatable.
 */
template <typename T>
void LinearModel<T>::create_fi() {
  const strvec& colnames = dt_X_fit_->get_names();
  dt::writable_string_col c_fi_names(nfeatures_);
  dt::writable_string_col::buffer_impl<uint32_t> sb(c_fi_names);
  sb.commit_and_start_new_chunk(0);
  for (const auto& feature_name : colnames) {
    sb.write(feature_name);
  }

  sb.order();
  sb.commit_and_start_new_chunk(nfeatures_);

  Column c_fi_values = Column::new_data_column(nfeatures_, stype_);
  dt_fi_ = dtptr(new DataTable({std::move(c_fi_names).to_ocolumn(), std::move(c_fi_values)},
                              {"feature_name", "feature_importance"})
                             );
  init_fi();
}


/**
 *  Initialize feature importances with zeros.
 */
template <typename T>
void LinearModel<T>::init_fi() {
  if (dt_fi_ == nullptr) return;
  auto data = static_cast<T*>(dt_fi_->get_column(1).get_data_editable());
  std::memset(data, 0, nfeatures_ * sizeof(T));
}


/**
 *  Return training status.
 */
template <typename T>
bool LinearModel<T>::is_fitted() {
  return dt_model_ != nullptr;
}


/**
 *  Get a shallow copy of a model if available.
 */
template <typename T>
py::oobj LinearModel<T>::get_model() {
  if (dt_model_ == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_model_));
}


/**
 *  Normalize a column of feature importances to [0; 1]
 *  This column has only positive values, so we simply divide its
 *  content by the maximum. Another option is to do min-max normalization,
 *  but this may lead to some features having zero importance,
 *  while in reality they don't.
 */
template <typename T>
py::oobj LinearModel<T>::get_fi(bool normalize /* = true */) {
  if (dt_fi_ == nullptr) return py::None();

  DataTable dt_fi__copy { *dt_fi_ };  // copy
  if (normalize) {
    Column& col = dt_fi__copy.get_column(1);
    bool max_isvalid;
    T max = static_cast<T>(col.stats()->max_double(&max_isvalid));
    T* data = static_cast<T*>(col.get_data_editable());

    if (max_isvalid && std::fabs(max) > T_EPSILON) {
      T norm_factor = T(1) / max;
      for (size_t i = 0; i < col.nrows(); ++i) {
        data[i] *= norm_factor;
      }
      col.reset_stats();
    }
  }
  return py::Frame::oframe(std::move(dt_fi__copy));
}


template <typename T>
size_t LinearModel<T>::get_nfeatures() {
  return nfeatures_;
}


template <typename T>
size_t LinearModel<T>::get_nlabels() {
  if (dt_labels_ != nullptr) {
    return dt_labels_->nrows();
  } else {
    return 0;
  }
}


template <typename T>
py::oobj LinearModel<T>::get_labels() {
  if (dt_labels_ == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_labels_));
}


template <typename T>
void LinearModel<T>::set_model(const DataTable& dt_model) {
  xassert(dt_model.nrows() > 1);
  dt_model_ = dtptr(new DataTable(dt_model));
}



template <typename T>
void LinearModel<T>::set_fi(const DataTable& dt_fi) {
  dt_fi_ = dtptr(new DataTable(dt_fi));
  nfeatures_ = dt_fi_->nrows();
}



template <typename T>
void LinearModel<T>::set_labels(const DataTable& dt_labels) {
  dt_labels_ = dtptr(new DataTable(dt_labels));
}


/**
 *  Sigmoid function.
 */
template <typename T>
T LinearModel<T>::activation_fn(T x) {
  return T(1) / (T(1) + std::exp(-x));
}


/**
 *  Logloss.
 */
template <typename T>
T LinearModel<T>::loss_fn(T p, T y) {
  constexpr T epsilon = std::numeric_limits<T>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  return -std::log(p * (2*y - 1) + 1 - y);
}


/**
 *  Target function for classification.
 */
template <typename T>
template <typename U>
T LinearModel<T>::target_fn(U y, size_t label_id) {
  return static_cast<T>(static_cast<size_t>(y) == label_id);
};


/**
 *  Target function for regression.
 */
template <typename T>
T LinearModel<T>::target_fn(T y, size_t) {
  return y;
};


template class LinearModel<float>;
template class LinearModel<double>;

template LinearModelFitOutput LinearModel<float>::fit_impl<int8_t>();
template LinearModelFitOutput LinearModel<float>::fit_impl<int32_t>();
template LinearModelFitOutput LinearModel<float>::fit_impl<float>();
template LinearModelFitOutput LinearModel<double>::fit_impl<int8_t>();
template LinearModelFitOutput LinearModel<double>::fit_impl<int32_t>();
template LinearModelFitOutput LinearModel<double>::fit_impl<double>();


} // namespace dt
