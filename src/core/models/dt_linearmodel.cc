//------------------------------------------------------------------------------
// Copyright 2021-2022 H2O.ai
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
  eta0_ = static_cast<T>(params->eta0);
  eta_decay_ = static_cast<T>(params->eta_decay);
  eta_drop_rate_ = static_cast<T>(params->eta_drop_rate);
  eta_schedule_ = params->eta_schedule;
  lambda1_ = static_cast<T>(params->lambda1);
  lambda2_ = static_cast<T>(params->lambda2);
  nepochs_ = static_cast<T>(params->nepochs);
  negative_class_ = params->negative_class;
  seed_ = params->seed;

  dt_X_fit_ = dt_X_fit;
  dt_y_fit_ = dt_y_fit;
  dt_X_val_ = dt_X_val;
  dt_y_val_ = dt_y_val;
  nepochs_val_ = static_cast<T>(nepochs_val);
  val_error_ = static_cast<T>(val_error);
  val_niters_ = val_niters;
  label_ids_fit_.clear(); label_ids_val_.clear();

  auto res = fit_model();

  dt_X_fit_ = nullptr;
  dt_y_fit_ = nullptr;
  dt_X_val_ = nullptr;
  dt_y_val_ = nullptr;
  nepochs_val_ = T_NAN;
  val_error_ = T_NAN;
  return res;
}


/**
 *  Fit a model by using ordinary least squares formulation with
 *  parallel stohastic gradient descent learning and elastic net regularization,
 *  see these references for more details:
 *  - https://en.wikipedia.org/wiki/Stochastic_gradient_descent
 *  - https://en.wikipedia.org/wiki/Elastic_net_regularization
 *  - http://martin.zinkevich.org/publications/nips2010.pdf
 */
template <typename T>
template <typename U> /* target column(s) data type */
LinearModelFitOutput LinearModel<T>::fit_impl() {
  // Initialization
  colvec cols = make_casted_columns(dt_X_fit_, stype_);
  if (!is_fitted()) {
    nfeatures_ = dt_X_fit_->ncols();
    create_model();
  }

  // Since `nepochs` can be a float value
  // - the model is trained `niterations - 1` times on
  //   `iteration_nrows` rows, where `iteration_nrows == dt_X_fit_->nrows()`;
  // - then, the model is trained once on the remaining `last_iteration_nrows` rows,
  //   where `0 < last_iteration_nrows <= dt_X_fit_->nrows()`.
  // If `nepochs_` is an integer number, `last_iteration_nrows == dt_X_fit_->nrows()`,
  // so that the last epoch becomes identical to all the others.
  size_t niterations = static_cast<size_t>(std::ceil(nepochs_));
  T last_epoch = nepochs_ - static_cast<T>(niterations) + 1;

  size_t iteration_nrows = dt_X_fit_->nrows();
  // Note: if iteration_nrows > 16.78M, this conversion loses precision
  T iteration_nrows_f = static_cast<T>(iteration_nrows);
  size_t last_iteration_nrows = static_cast<size_t>(last_epoch * iteration_nrows_f);
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
    iteration_nrows = static_cast<size_t>(std::ceil(nepochs_val_ * iteration_nrows_f));
    niterations = total_nrows / iteration_nrows + (total_nrows % iteration_nrows > 0);
    loss_history.resize(val_niters_, 0.0);
  }

  // Mutex for single-threaded regions
  std::mutex m;

  // Work amounts for full fit iterations, last fit iteration and validation
  size_t work_total = (niterations - 1) * get_work_amount(iteration_nrows, MIN_ROWS_PER_THREAD);
  work_total += get_work_amount(total_nrows - (niterations - 1) * iteration_nrows, MIN_ROWS_PER_THREAD);
  if (validation) work_total += niterations * get_work_amount(dt_X_val_->nrows(), MIN_ROWS_PER_THREAD);

  // Set work amount to be reported by the zero thread
  dt::progress::work job(work_total);
  job.set_message("Fitting...");
  NThreads nthreads = nthreads_from_niters(iteration_nrows, MIN_ROWS_PER_THREAD);

  // Calculate parameters for the modular quasi-random generator.
  // By default, when seed is zero, modular_random_gen() will return
  // multiplier == 1 and increment == 0, so we don't do any data shuffling,
  auto mp = modular_random_gen(dt_X_fit_->nrows(), seed_);
  T eta = eta0_;

  dt::parallel_region(nthreads,
    [&]() {
      // Each thread gets a private storage for observations and feature importances.
      tptr<T> x = tptr<T>(new T[nfeatures_]);
      std::vector<std::vector<T>> data_container;
      std::vector<T*> betas;
      size_t ncols = dt_model_->ncols();
      size_t nrows = dt_model_->nrows();
      data_container.resize(ncols);
      betas.resize(ncols);
      for (size_t i = 0; i < ncols; i++) {
        data_container[i].resize(nrows);
        betas[i] = data_container[i].data();
      }

      for (size_t iter = 0; iter < niterations; ++iter) {
        // Each thread gets its own copy of the model
        for (size_t i = 0; i < ncols; i++) {
          const auto* data = static_cast<const T*>(
            dt_model_->get_column(i).get_data_readonly());
          for (size_t j = 0; j < nrows; j++) {
            betas[i][j] = data[j];
          }
        }

        size_t iteration_start = iter * iteration_nrows;
        iteration_end = (iter == niterations - 1)? total_nrows :
                                                   (iter + 1) * iteration_nrows;
        size_t iteration_size = iteration_end - iteration_start;

        // Training
        dt::nested_for_static(iteration_size, ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
          // Do quasi-random data shuffling
          size_t ii = ((iteration_start + i) * mp.multiplier + mp.increment) % dt_X_fit_->nrows();
          U target;
          bool isvalid = col_y_fit_.get_element(ii, &target);
          if (isvalid && _isfinite(target) && read_row(ii, cols, x)) {
            // Loop over all the labels
            for (size_t k = 0; k < label_ids_fit_.size(); ++k) {
              T p = activation_fn(predict_row(x, betas, k));
              T y = target_fn(target, label_ids_fit_[k]);
              T delta = p - y;

              // Update local betas with SGD, j = 0 corresponds to the bias term
              for (size_t j = 0; j < nfeatures_ + 1; ++j) {
                // If we use sigmoid activation, gradients for linear and logistic
                // regressions are the same. For other activations the gradient
                // should be adjusted accordingly.
                T gradient = delta;
                if (j) gradient *= x[j - 1];
                gradient += copysign(lambda1_, betas[k][j]); // L1 regularization
                gradient += 2 * lambda2_ * betas[k][j];      // L2 regularization

                if (_isfinite(gradient)) {
                  betas[k][j] -= eta * gradient;
                }
              }
            }
          }

          // Report progress
          if (dt::this_thread_index() == 0) {
            job.add_done_amount(1);
          }

        }); // End training
        barrier();

        // Update global model coefficients by averaging the local ones.
        // Also adjust `eta` according to the schedule.
        {
          // First, zero out the global model and update `eta`.
          if (dt::this_thread_index() == 0) {
            init_model();
            betas_ = get_model_data(dt_model_);
            adjust_eta(eta, iter + 1);
          }
          barrier();

          // Second, average the local coefficients
          {
            std::lock_guard<std::mutex> lock(m);
            auto nth = static_cast<T>(dt::num_threads_in_team());
            for (size_t i = 0; i < ncols; ++i) {
              for (size_t j = 0; j < nrows; ++j) {
                betas_[i][j] += betas[i][j] / nth;
              }
            }
          }
        }
        barrier();

        // Validation and early stopping
        if (validation) {
          dt::atomic<T> loss_global {0.0};
          T loss_local = 0.0;

          dt::nested_for_static(dt_X_val_->nrows(), ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
            U y_val;
            bool isvalid = col_y_val_.get_element(i, &y_val);

            if (isvalid && _isfinite(y_val) && read_row(i, cols_val, x)) {
              for (size_t k = 0; k < label_ids_val_.size(); ++k) {
                T p = activation_fn(predict_row(x, betas_, k));
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

          double epoch = static_cast<double>(iteration_end) / static_cast<double>(dt_X_fit_->nrows());
          if (std::isnan(loss_old)) {
            if (dt::this_thread_index() == 0) {
              job.set_message("Stopping at epoch " + tostr(epoch) +
                              ", loss = " + tostr(loss));
              // In some cases this makes progress "jumping" to 100%
              job.set_done_amount(work_total);
            }
            break;
          }
          if (dt::this_thread_index() == 0) {
            job.set_message("Fitting... epoch " + tostr(epoch) +
                            " of " + tostr(nepochs_) + ", loss = " + tostr(loss));
          }
        } // End validation

      } // End iteration
    }
  );
  job.done();

  double epoch_stopped = static_cast<double>(iteration_end) / static_cast<double>(dt_X_fit_->nrows());
  LinearModelFitOutput res = {epoch_stopped, static_cast<double>(loss)};

  return res;
}


/**
 *  Calculate learning rate for a particular schedule.
 */
template <typename T>
void LinearModel<T>::adjust_eta(T& eta, size_t iter) {
  switch (eta_schedule_) {
    case LearningRateSchedule::CONSTANT:
      eta = eta0_;
      break;
    case LearningRateSchedule::TIME_BASED:
      eta = eta0_ / (1 + eta_decay_ * static_cast<T>(iter));
      break;
    case LearningRateSchedule::STEP_BASED:
      eta = eta0_ * std::pow(eta_decay_, std::floor(static_cast<T>(1 + iter) / eta_drop_rate_));
      break;
    case LearningRateSchedule::EXPONENTIAL:
      eta = eta0_ / std::exp(eta_decay_ * static_cast<T>(iter));
  }
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
 *  Predict for one row.
 */
template <typename T>
T LinearModel<T>::predict_row(const tptr<T>& x,
                              const std::vector<T*> betas,
                              const size_t k)
{
  T wTx = betas[k][0];
  for (size_t i = 0; i < nfeatures_; ++i) {
    wTx += betas[k][i + 1] * x[i];
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
  betas_ = get_model_data(dt_model_);
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
          // Note, binomial classifier may adjust `k` to match the label
          // with the positive class
          size_t label_id = get_label_id(k, data_label_ids);
          data_p[k][i] = activation_fn(predict_row(x, betas_, label_id));
        }
      } else {
        for (size_t k = 0; k < get_nclasses(); ++k) {
          data_p[k][i] = T_NAN;
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
 *  Create a model and initialize coefficients.
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
 *  Obtain pointers to the model data.
 */
template <typename T>
std::vector<T*> LinearModel<T>::get_model_data(const dtptr& dt) {
  size_t nlabels = dt->ncols();
  std::vector<T*> betas;
  betas.reserve(nlabels);

  for (size_t k = 0; k < nlabels; ++k) {
    betas.push_back(static_cast<T*>(dt->get_column(k).get_data_editable()));
  }

  return betas;
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
  nfeatures_ = dt_model.nrows() - 1;
}


template <typename T>
void LinearModel<T>::set_labels(const DataTable& dt_labels) {
  dt_labels_ = dtptr(new DataTable(dt_labels));
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
