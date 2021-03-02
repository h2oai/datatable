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
#include "wstringcol.h"
#include "utils/macros.h"


namespace dt {


/**
 *  Constructor with default model parameters.
 */
template <typename T>
LinearModel<T>::LinearModel() : LinearModel(LinearModelParams()) {}


/**
 *  Constructor with arbitrary model parameters.
 */
template <typename T>
LinearModel<T>::LinearModel(LinearModelParams params_in) :
  stype(sizeof(T) == 4? dt::SType::FLOAT32 : dt::SType::FLOAT64),
  model_type(LinearModelType::NONE), // `NONE` means model was not trained
  params(params_in),
  eta(static_cast<T>(params_in.eta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nepochs(static_cast<T>(params_in.nepochs)),
  nfeatures(0),
  dt_X_train(nullptr),
  dt_y_train(nullptr),
  dt_X_val(nullptr),
  dt_y_val(nullptr),
  nepochs_val(T_NAN),
  val_error(T_NAN),
  val_niters(0)
{
}


/**
 *  Depending on a requested problem type, this method calls
 *  an appropriate `fit_*` method, and returns epoch at which
 *  training stopped, and the corresponding loss.
 */
template <typename T>
LinearModelFitOutput LinearModel<T>::dispatch_fit(const DataTable* dt_X_train_in,
                                    const DataTable* dt_y_train_in,
                                    const DataTable* dt_X_val_in,
                                    const DataTable* dt_y_val_in,
                                    double nepochs_val_in,
                                    double val_error_in,
                                    size_t val_niters_in) {
  dt_X_train = dt_X_train_in;
  dt_y_train = dt_y_train_in;
  dt_X_val = dt_X_val_in;
  dt_y_val = dt_y_val_in;
  nepochs_val = static_cast<T>(nepochs_val_in);
  val_error = static_cast<T>(val_error_in);
  val_niters = val_niters_in;
  label_ids_train.clear();
  label_ids_val.clear();
  LinearModelFitOutput res;

  SType stype_y = dt_y_train->get_column(0).stype();
  LinearModelType model_type_train = !is_model_trained()? params.model_type :
                                                          model_type;

  if (!is_model_trained()) define_features();

  xassert(model_type_train != LinearModelType::NONE);
  switch (model_type_train) {
    case LinearModelType::AUTO :        switch (stype_y) {
                                        case SType::VOID:
                                        case SType::BOOL:    res = fit_binomial(); break;
                                        case SType::INT8:    res = fit_regression<int8_t>(); break;
                                        case SType::INT16:   res = fit_regression<int16_t>(); break;
                                        case SType::INT32:   res = fit_regression<int32_t>(); break;
                                        case SType::INT64:   res = fit_regression<int64_t>(); break;
                                        case SType::FLOAT32: res = fit_regression<float>(); break;
                                        case SType::FLOAT64: res = fit_regression<double>(); break;
                                        case SType::STR32:
                                        case SType::STR64:   res = fit_multinomial(); break;
                                        default:             throw TypeError() << "Target column type `"
                                                                               << stype_y << "` is not supported";
                                      }
                                      break;

    case LinearModelType::REGRESSION :  switch (stype_y) {
                                        case SType::VOID:
                                        case SType::BOOL:    res = fit_regression<int8_t>(); break;
                                        case SType::INT8:    res = fit_regression<int8_t>(); break;
                                        case SType::INT16:   res = fit_regression<int16_t>(); break;
                                        case SType::INT32:   res = fit_regression<int32_t>(); break;
                                        case SType::INT64:   res = fit_regression<int64_t>(); break;
                                        case SType::FLOAT32: res = fit_regression<float>(); break;
                                        case SType::FLOAT64: res = fit_regression<double>(); break;
                                        default:             throw TypeError() << "Target column type `"
                                                                               << stype_y << "` is not supported by "
                                                                               << "the numeric regression";
                                      }
                                      break;

    case LinearModelType::BINOMIAL :    res = fit_binomial(); break;
    case LinearModelType::MULTINOMIAL : res = fit_multinomial(); break;
    case LinearModelType::NONE : throw ValueError() << "Cannot train model in an unknown mode";
  }

  dt_X_train = nullptr;
  dt_y_train = nullptr;
  dt_X_val = nullptr;
  dt_y_val = nullptr;
  nepochs_val = T_NAN;
  val_error = T_NAN;
  return res;
}


/**
 *  Prepare data for binomial problem, and call the main fit method.
 */
template <typename T>
LinearModelFitOutput LinearModel<T>::fit_binomial() {
  dtptr dt_y_train_binomial, dt_y_val_binomial;
  bool validation = _notnan(nepochs_val);
  create_y_binomial(dt_y_train, dt_y_train_binomial, label_ids_train, dt_labels);

  // NA values are ignored during training, so we stop training right away,
  // if got NA's only.
  if (dt_y_train_binomial == nullptr) return {0, static_cast<double>(T_NAN)};
  dt_y_train = dt_y_train_binomial.get();

  if (validation) {
    create_y_binomial(dt_y_val, dt_y_val_binomial, label_ids_val, dt_labels);
    if (dt_y_val_binomial == nullptr) {
      throw ValueError() << "Cannot set early stopping criteria as validation "
                            "target column got `NA` targets only";
    }
    dt_y_val = dt_y_val_binomial.get();
  }

  if (!is_model_trained()) {
    model_type = LinearModelType::BINOMIAL;
    create_model();
  }

  // For binomial regression training and validation target columns
  // are both SType::BOOL, see `label_encode()` implementation for more
  // details.
  auto targetfn = [] (int8_t y, size_t label_id) -> int8_t {
                       return static_cast<size_t>(y) == label_id;
                     };
  return fit<int8_t, int8_t>(sigmoid<T>, dsigmoid<T>, targetfn, targetfn, log_loss<T>);
}


/**
 *  Create labels (in the case of numeric regression there is no actual
 *  labeles, so we just use a column name for this purpose),
 *  set up identity mapping between models and the incoming label inficators,
 *  call to the main `fit` method.
 */
template <typename T>
template <typename U>
LinearModelFitOutput LinearModel<T>::fit_regression() {
  xassert(dt_y_train->ncols() == 1);
  if (is_model_trained() && model_type != LinearModelType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }

  if (!is_model_trained()) {
    const strvec& colnames = dt_y_train->get_names();
    std::unordered_map<std::string, int32_t> colnames_map = {{colnames[0], 0}};
    dt_labels = create_dt_labels_str<uint32_t>(colnames_map);

    create_model();
    model_type = LinearModelType::REGRESSION;
  }

  label_ids_train = { 0 };
  label_ids_val = { 0 };

  // For numeric regression `targetfn` is just an identity function
  auto targetfn = [](auto y, size_t) {
    return y;
  };

  LinearModelFitOutput res;

  if (_notnan(nepochs_val)) {
    // If we got validation datasets, figure out stype of
    // the validation target column and make an appropriate call to `.fit()`.
    SType stype_y_val = dt_y_val->get_column(0).stype();
    switch (stype_y_val) {
      case SType::VOID:
      case SType::BOOL:    res = fit<U, int8_t>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::INT8:    res = fit<U, int8_t>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::INT16:   res = fit<U, int16_t>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::INT32:   res = fit<U, int32_t>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::INT64:   res = fit<U, int64_t>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::FLOAT32: res = fit<U, float>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      case SType::FLOAT64: res = fit<U, double>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>); break;
      default:             throw TypeError() << "Target column type `"
                                             << stype_y_val << "` is not supported by numeric regression";
    }
  } else {
    // If no validation was requested, it doesn't matter
    // what validation type we are passing to the `fit()` method.
    res = fit<U, U>(identity<T>, didentity<T>, targetfn, targetfn, squared_loss<T>);
  }

  return res;
}


/**
 *  Prepare data for multinomial problem, and call the main fit method.
 */
template <typename T>
LinearModelFitOutput LinearModel<T>::fit_multinomial() {
  if (is_model_trained() && model_type != LinearModelType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a multinomial mode this model should be reset.";
  }


  dtptr dt_y_train_multinomial;
  size_t n_new_labels = create_y_multinomial(
                          dt_y_train, dt_y_train_multinomial, label_ids_train,
                          dt_labels, params.negative_class
                        );

  // Adjust trained model if we got new labels
  if (n_new_labels) {
    xassert(is_model_trained());
    adjust_model();
  }

  if (dt_y_train_multinomial == nullptr) return {0, static_cast<double>(T_NAN)};
  dt_y_train = dt_y_train_multinomial.get();

  // Create validation targets if needed.
  dtptr dt_y_val_multinomial;
  if (_notnan(nepochs_val)) {
    create_y_multinomial(dt_y_val, dt_y_val_multinomial, label_ids_val, dt_labels,
                         params.negative_class, true);
    if (dt_y_val_multinomial == nullptr)
      throw ValueError() << "Cannot set early stopping criteria as validation "
                         << "target column got `NA` targets only";
    dt_y_val = dt_y_val_multinomial.get();
  }

  if (!is_model_trained()) {
    xassert(dt_model == nullptr);
    create_model();
    model_type = LinearModelType::MULTINOMIAL;
  }

  // For binomial regression training and validation target columns
  // are both SType::INT32, see `label_encode()` implementation for more
  // details.
  auto targetfn = [] (int32_t y, size_t label_indicator) -> int32_t {
                       return static_cast<size_t>(y) == label_indicator;
                     };
  return fit<int32_t, int32_t>(sigmoid<T>, dsigmoid<T>, targetfn, targetfn, log_loss<T>);
}


/**
 *  Add negative class label and save its model id.
 */
template <typename T>
void LinearModel<T>::add_negative_class() {
  dt::writable_string_col c_labels(1);
  dt::writable_string_col::buffer_impl<uint32_t> sb(c_labels);
  sb.commit_and_start_new_chunk(0);
  sb.write("_negative_class");
  sb.order();
  sb.commit_and_start_new_chunk(1);

  Column c_ids = Column::new_data_column(1, SType::INT32);
  auto d_ids = static_cast<int32_t*>(c_ids.get_data_editable());
  d_ids[0] = 0;

  dtptr dt_nc = dtptr(
                  new DataTable(
                    {std::move(c_labels).to_ocolumn(), std::move(c_ids)},
                    dt_labels->get_names()
                  )
                );

  dt_labels->clear_key();

  // Increment all the exiting label ids, and then insert
  // a `_negative_class` label with the zero id.
  adjust_values<int32_t>(
    dt_labels->get_column(1),
    [](int32_t& value, size_t) {
      ++value;
    }
  );

  dt_labels->rbind({dt_nc.get()}, {{ 0 } , { 1 }});
  sztvec keys{ 0 };
  dt_labels->set_key(keys);
}


/**
 *  Fit model on a datatable.
 */
template <typename T>
template <typename U, typename V> /* target column(s) data type */
LinearModelFitOutput LinearModel<T>::fit(T(*linkfn)(T),
                                         T(*dlinkfn)(T),
                                         U(*targetfn)(U, size_t),
                                         V(*targetfn_val)(V, size_t),
                                         T(*lossfn)(T, T))
{
  // Define weight pointers, feature importances storage
  init_coefficients();
  if (dt_fi == nullptr) create_fi();
  colvec cols = make_casted_columns(dt_X_train, stype);


  // Obtain rowindex and data pointers for the target column(s).
  const Column& target_col0_train = dt_y_train->get_column(0);
  auto data_fi = static_cast<T*>(dt_fi->get_column(1).get_data_editable());

  // Since `nepochs` can be a float value
  // - the model is trained `niterations - 1` times on
  //   `iteration_nrows` rows, where `iteration_nrows == dt_X_train->nrows()`;
  // - then, the model is trained on the remaining `last_iteration_nrows` rows,
  //   where `0 < last_iteration_nrows <= dt_X_train->nrows()`.
  // If `nepochs` is an integer number, `last_iteration_nrows == dt_X_train->nrows()`,
  // so that the last epoch becomes identical to all the others.
  size_t niterations = static_cast<size_t>(std::ceil(nepochs));
  T last_epoch = nepochs - static_cast<T>(niterations) + 1;

  size_t iteration_nrows = dt_X_train->nrows();
  size_t last_iteration_nrows = static_cast<size_t>(last_epoch * static_cast<T>(iteration_nrows));
  size_t total_nrows = (niterations - 1) * iteration_nrows + last_iteration_nrows;
  size_t iteration_end;

  // If a validation set is provided, we adjust the `iteration_nrows`
  // to correspond to the `nepochs_val` epochs. After each iteration, we calculate
  // the loss on the validation dataset, and trigger early stopping
  // if relative loss does not decrese by at least `val_error`.
  bool validation = _notnan(nepochs_val);
  T loss = T_NAN;    // This value is returned when validation is not enabled
  T loss_old = T(0); // Value of `loss` for a previous iteraction
  std::vector<T> loss_history;
  const Column& target_col0_val = validation? dt_y_val->get_column(0)
                                            : target_col0_train;  // whatever
  colvec cols_val;

  if (validation) {
    cols_val = make_casted_columns(dt_X_val, stype);
    iteration_nrows = static_cast<size_t>(std::ceil(nepochs_val * static_cast<T>(iteration_nrows)));
    niterations = total_nrows / iteration_nrows + (total_nrows % iteration_nrows > 0);
    loss_history.resize(val_niters, 0.0);
  }


  // Mutex to update global feature importance information
  std::mutex m;

  // Calculate work amounts for full fit iterations, last fit iteration and
  // validation.
  size_t work_total = (niterations - 1) * get_work_amount(iteration_nrows);
  work_total += get_work_amount(total_nrows - (niterations - 1) * iteration_nrows);
  if (validation) work_total += niterations * get_work_amount(dt_X_val->nrows());

  // Set work amount to be reported by the zero thread.
  dt::progress::work job(work_total);
  job.set_message("Fitting...");
  NThreads nthreads = nthreads_from_niters(iteration_nrows, MIN_ROWS_PER_THREAD);

  dt::parallel_region(nthreads,
    [&]() {
      // Each thread gets a private storage observations and feature importances.
      tptr<T> x = tptr<T>(new T[nfeatures]);
      tptr<T> fi = tptr<T>(new T[nfeatures]());

      for (size_t iter = 0; iter < niterations; ++iter) {
        size_t iteration_start = iter * iteration_nrows;
        iteration_end = (iter == niterations - 1)? total_nrows :
                                                   (iter + 1) * iteration_nrows;
        size_t iteration_size = iteration_end - iteration_start;

        // Training.
        dt::nested_for_static(iteration_size, ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
          size_t ii = (iteration_start + i) % dt_X_train->nrows();
          U target;
          bool isvalid = target_col0_train.get_element(ii, &target);

          if (isvalid && _isfinite(target) && read_row(ii, cols, x)) {
            // Loop over all the labels
            for (size_t k = 0; k < label_ids_train.size(); ++k) {
              // Update all the coefficients with SGD
              for (size_t j = 0; j < nfeatures + 1; ++j) {

                T grad = linkfn(predict_row(x, k,
                           [&](size_t f_id, T f_imp) {
                             fi[f_id] += f_imp;
                            }
                         ));
                T y = static_cast<T>(targetfn(target, label_ids_train[k]));
                grad = 2 * dlinkfn(grad) * (grad - y);
                if (j) grad *= x[j - 1];
                grad += copysign(lambda1, betas[k][j]); // L1 regularization
                grad += 2 * lambda2 * betas[k][j];      // L2 regularization

                // Update only when the final `grad` is finite
                if (_isfinite(grad)) {
                  betas[k][j] -= eta * grad;
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

          dt::nested_for_static(dt_X_val->nrows(), ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
            V target;
            bool isvalid = target_col0_val.get_element(i, &target);

            if (isvalid && _isfinite(target) && read_row(i, cols_val, x)) {
              for (size_t k = 0; k < label_ids_val.size(); ++k) {
                T p = linkfn(predict_row(
                        x, k, [&](size_t, T){}
                      ));
                V y = targetfn_val(target, label_ids_val[k]);
                loss_local += lossfn(p, static_cast<T>(y));
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
          // more than `val_error`, sets `loss_old` to `NaN` -> this will stop
          // all the threads after `barrier()`.
          if (dt::this_thread_index() == 0) {
            T loss_current = loss_global.load() / static_cast<T>(dt_X_val->nrows() * dt_y_val->ncols());
            loss_history[iter % val_niters] = loss_current / static_cast<T>(val_niters);
            loss = std::accumulate(loss_history.begin(), loss_history.end(), T(0));
            T loss_diff = (loss_old - loss) / loss_old;
            bool is_loss_bad = (iter >= val_niters) && (loss < T_EPSILON || loss_diff < val_error);
            loss_old = is_loss_bad? T_NAN : loss;
          }
          barrier();

          double epoch = static_cast<double>(iteration_end) / static_cast<double>(dt_X_train->nrows());
          if (std::isnan(loss_old)) {
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
                            " of " + tostr(nepochs) +
                            ", loss = " + tostr(loss));
          }
        } // End validation

        // Update global feature importances with the local data.
        std::lock_guard<std::mutex> lock(m);
        for (size_t i = 0; i < nfeatures; ++i) {
          data_fi[i] += fi[i];
        }

      } // End iteration.

    }
  );
  job.done();

  double epoch_stopped = static_cast<double>(iteration_end) / static_cast<double>(dt_X_train->nrows());
  LinearModelFitOutput res = {epoch_stopped, static_cast<double>(loss)};

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
 *  Make a prediction and update feature importances if requested.
 */
template <typename T>
template <typename F>
T LinearModel<T>::predict_row(const tptr<T>& x, const size_t k, F fifn) {
  T wTx = betas[k][0];
  for (size_t i = 0; i < nfeatures; ++i) {
    wTx += betas[k][i + 1] * x[i];
    fifn(i, abs(betas[k][i + 1] * x[i]));
  }
  return wTx;
}



/**
 *  Predict on a datatable and return a new datatable with
 *  the predicted probabilities.
 */
template <typename T>
dtptr LinearModel<T>::predict(const DataTable* dt_X) {
  if (!is_model_trained()) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }

  // Re-acquire model weight pointers.
  init_coefficients();
  colvec cols = make_casted_columns(dt_X, stype);

  // Create datatable for predictions and obtain column data pointers.
  size_t nlabels = dt_labels->nrows();

  auto data_label_ids = static_cast<const int32_t*>(
                          dt_labels->get_column(1).get_data_readonly()
                        );

  dtptr dt_p = create_p(dt_X->nrows());
  std::vector<T*> data_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    data_p[i] = static_cast<T*>(dt_p->get_column(i).get_data_editable());
  }

  // Determine which link function we should use.
  T (*linkfn)(T);
  switch (model_type) {
    case LinearModelType::REGRESSION  : linkfn = identity<T>; break;
    case LinearModelType::BINOMIAL    : linkfn = sigmoid<T>; break;
    case LinearModelType::MULTINOMIAL : (nlabels < 3)? linkfn = sigmoid<T> :
                                                       linkfn = identity<T>;
                                      break;
    default : throw ValueError() << "Cannot do any predictions, "
                                 << "the model was trained in an unknown mode";
  }

  NThreads nthreads = nthreads_from_niters(dt_X->nrows(), MIN_ROWS_PER_THREAD);
  bool k_binomial;

  // Set progress reporting
  size_t work_total = get_work_amount(dt_X->nrows());

  dt::progress::work job(work_total);
  job.set_message("Predicting...");

  dt::parallel_region(NThreads(nthreads), [&]() {
    tptr<T> x = tptr<T>(new T[nfeatures]);

    dt::nested_for_static(dt_X->nrows(), ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
      // Predicting for all the `nlabels`
      if (read_row(i, cols, x)) {
        for (size_t k = 0; k < nlabels; ++k) {
          size_t label_id = static_cast<size_t>(data_label_ids[k]);
          if (model_type == LinearModelType::BINOMIAL && label_id == 1) {
            k_binomial = k;
            continue;
          }

          data_p[k][i] = linkfn(predict_row(x, label_id, [&](size_t, T){}));
        }
      }

      // Progress reporting
      if (dt::this_thread_index() == 0) {
        job.add_done_amount(1);
      }
    });
  });
  job.done();

  if (model_type == LinearModelType::BINOMIAL) {
    dt::parallel_for_static(dt_X->nrows(), [&](size_t i){
      data_p[k_binomial][i] = T(1) - data_p[!k_binomial][i];
    });
  }

  // For multinomial case, when there is only two labels, we match binomial
  // classifier by using `sigmoid` link function. When there is more
  // than two labels, we first employ `identity` linking, and do `softmax`
  // normalization at the end.
  if (nlabels > 2) softmax_rows(data_p, dt_p->nrows());
  return dt_p;
}


/**
 *  Normalize predictions, so that their values sum up to `1` row-wise.
 *  To prevent overflow when calculating the softmax function,
 *  we multiply its numerator and denominator by `std::exp(-max)`,
 *  where `max` is the maximum value of predictions for a given row.
 */
template <typename T>
void LinearModel<T>::softmax_rows(std::vector<T*>& data_p, const size_t nrows) {
  size_t ncols = data_p.size();

  dt::parallel_for_static(nrows, [&](size_t i){
    T sum = T(0);
    T max = data_p[0][i];
    for (size_t j = 1; j < ncols; ++j) {
      if (data_p[j][i] > max) max = data_p[j][i];
    }

    for (size_t j = 0; j < ncols; ++j) {
      data_p[j][i] = std::exp(data_p[j][i] - max);
      sum += data_p[j][i];
    }
    for (size_t j = 0; j < ncols; ++j) {
      data_p[j][i] /= sum;
    }
  });
}


/**
 *  Create a datatable of shape (nfeatures + 1, nlabels) to store coefficients.
 */
template <typename T>
void LinearModel<T>::create_model() {
  size_t nlabels = (dt_labels == nullptr)? 0 : dt_labels->nrows();
  size_t ncols = (model_type == LinearModelType::BINOMIAL)? 1 : nlabels;

  colvec cols;
  cols.reserve(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    cols.push_back(Column::new_data_column(nfeatures + 1, stype));
  }
  dt_model = dtptr(new DataTable(std::move(cols), DataTable::default_names));
  init_model();
}


/**
 *  This method is invoked in the case when we get new labels
 *  for multinomial classification and need to add them to the model.
 *  In such a case, we make a copy of the "negative" coefficients
 *  adding them to the existing `dt_model` columns.
 */
template <typename T>
void LinearModel<T>::adjust_model() {
  size_t ncols_model = dt_model->ncols();
  size_t ncols_model_new = dt_labels->nrows();
  xassert(ncols_model_new > ncols_model);

  colvec cols;
  cols.reserve(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols.push_back(dt_model->get_column(i));
  }

  Column wcol;
  // If `negative_class` parameter is set to `True`, all the new classes
  // get a copy of `w` coefficients of the `_negative_class`.
  // Otherwise, new classes start learning from the zero coefficients.
  if (params.negative_class) {
    wcol = dt_model->get_column(0);
  } else {
    Column col = Column::new_data_column(nfeatures + 1, stype);
    auto data = static_cast<T*>(col.get_data_editable());
    std::memset(data, 0, (nfeatures + 1) * sizeof(T));
    wcol = col;
  }

  for (size_t i = ncols_model; i < ncols_model_new; ++i) {
    cols.push_back(wcol);
  }

  dt_model = dtptr(new DataTable(std::move(cols), DataTable::default_names));
}


/**
 *  Create datatable for predictions.
 */
template <typename T>
dtptr LinearModel<T>::create_p(size_t nrows) {
  size_t nlabels = dt_labels->nrows();
  xassert(nlabels > 0);

  Column col0_str64 = dt_labels->get_column(0).cast(SType::STR64);

  strvec labels_vec(nlabels);

  for (size_t i = 0; i < nlabels; ++i) {
    CString val;
    bool isvalid = col0_str64.get_element(i, &val);
    labels_vec[i] = isvalid? val.to_string() : std::string();
  }

  colvec cols;
  cols.reserve(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    cols.push_back(Column::new_data_column(nrows, stype));
  }

  dtptr dt_p = dtptr(new DataTable(std::move(cols), std::move(labels_vec)));
  return dt_p;
}


/**
 *  Reset the model.
 */
template <typename T>
void LinearModel<T>::reset() {
  dt_model = nullptr;
  dt_fi = nullptr;
  model_type = LinearModelType::NONE;
  dt_labels = nullptr;
}


/**
 *  Initialize model coefficients with zeros.
 */
template <typename T>
void LinearModel<T>::init_model() {
  if (dt_model == nullptr) return;
  xassert(dt_model->nrows() == nfeatures + 1);
  for (size_t i = 0; i < dt_model->ncols(); ++i) {
    auto data = static_cast<T*>(dt_model->get_column(i).get_data_editable());
    std::memset(data, 0, (nfeatures + 1) * sizeof(T));
  }
}


/**
 *  Obtain pointers to the model column data.
 */
template <typename T>
void LinearModel<T>::init_coefficients() {
  size_t nlabels = dt_model->ncols();
  betas.clear();
  betas.reserve(nlabels);

  for (size_t k = 0; k < nlabels; ++k) {
    betas.push_back(static_cast<T*>(dt_model->get_column(k).get_data_editable()));
  }
}


/**
 * Create feature importance datatable.
 */
template <typename T>
void LinearModel<T>::create_fi() {
  const strvec& colnames = dt_X_train->get_names();

  dt::writable_string_col c_fi_names(nfeatures);
  dt::writable_string_col::buffer_impl<uint32_t> sb(c_fi_names);
  sb.commit_and_start_new_chunk(0);
  for (const auto& feature_name : colnames) {
    sb.write(feature_name);
  }

  sb.order();
  sb.commit_and_start_new_chunk(nfeatures);

  Column c_fi_values = Column::new_data_column(nfeatures, stype);
  dt_fi = dtptr(new DataTable({std::move(c_fi_names).to_ocolumn(), std::move(c_fi_values)},
                              {"feature_name", "feature_importance"})
                             );
  init_fi();
}


/**
 *  Initialize feature importances with zeros.
 */
template <typename T>
void LinearModel<T>::init_fi() {
  if (dt_fi == nullptr) return;
  auto data = static_cast<T*>(dt_fi->get_column(1).get_data_editable());
  std::memset(data, 0, nfeatures * sizeof(T));
}


/**
 *  Determine number of features.
 */
template <typename T>
void LinearModel<T>::define_features() {
  nfeatures = dt_X_train->ncols();
}



/**
 *  Return training status.
 */
template <typename T>
bool LinearModel<T>::is_model_trained() {
  return model_type != LinearModelType::NONE;
}


/**
 *  Get a shallow copy of a model if available.
 */
template <typename T>
py::oobj LinearModel<T>::get_model() {
  if (dt_model == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_model));
}


/**
 *  Return model type.
 */
template <typename T>
LinearModelType LinearModel<T>::get_model_type() {
  return params.model_type;
}


/**
 *  Return trained model type.
 */
template <typename T>
LinearModelType LinearModel<T>::get_model_type_trained() {
  return model_type;
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
  if (dt_fi == nullptr) return py::None();

  DataTable dt_fi_copy { *dt_fi };  // copy
  if (normalize) {
    Column& col = dt_fi_copy.get_column(1);
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
  return py::Frame::oframe(std::move(dt_fi_copy));
}


template <typename T>
size_t LinearModel<T>::get_ncols() {
  return nfeatures;
}


template <typename T>
size_t LinearModel<T>::get_nfeatures() {
  return nfeatures;
}


template <typename T>
size_t LinearModel<T>::get_nlabels() {
  if (dt_labels != nullptr) return dt_labels->nrows();
  else return 0;
  return nfeatures;
}


template <typename T>
double LinearModel<T>::get_eta() {
  return params.eta;
}


template <typename T>
double LinearModel<T>::get_lambda1() {
  return params.lambda1;
}


template <typename T>
double LinearModel<T>::get_lambda2() {
  return params.lambda2;
}


template <typename T>
double LinearModel<T>::get_nepochs() {
  return params.nepochs;
}


template <typename T>
bool LinearModel<T>::get_negative_class() {
  return params.negative_class;
}


template <typename T>
LinearModelParams LinearModel<T>::get_params() {
  return params;
}


template <typename T>
py::oobj LinearModel<T>::get_labels() {
  if (dt_labels == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_labels));
}


template <typename T>
void LinearModel<T>::set_model(const DataTable& dt_model_in) {
  dt_model = dtptr(new DataTable(dt_model_in));
  xassert(dt_model_in.nrows() > 1);
}


template <typename T>
void LinearModel<T>::set_model_type(LinearModelType model_type_in) {
  params.model_type = model_type_in;
}


template <typename T>
void LinearModel<T>::set_model_type_trained(LinearModelType model_type_trained_in) {
  model_type = model_type_trained_in;
}


template <typename T>
void LinearModel<T>::set_fi(const DataTable& dt_fi_in) {
  dt_fi = dtptr(new DataTable(dt_fi_in));
  nfeatures = dt_fi->nrows();
}



template <typename T>
void LinearModel<T>::set_eta(double eta_in) {
  params.eta = eta_in;
  eta = static_cast<T>(eta_in);
}


template <typename T>
void LinearModel<T>::set_lambda1(double lambda1_in) {
  params.lambda1 = lambda1_in;
  lambda1 = static_cast<T>(lambda1_in);
}


template <typename T>
void LinearModel<T>::set_lambda2(double lambda2_in) {
  params.lambda2 = lambda2_in;
  lambda2 = static_cast<T>(lambda2_in);
}


template <typename T>
void LinearModel<T>::set_nepochs(double nepochs_in) {
  params.nepochs = nepochs_in;
  nepochs = static_cast<T>(nepochs_in);
}


template <typename T>
void LinearModel<T>::set_negative_class(bool negative_class_in) {
  params.negative_class = negative_class_in;
}


template <typename T>
void LinearModel<T>::set_labels(const DataTable& dt_labels_in) {
  dt_labels = dtptr(new DataTable(dt_labels_in));
}


template class LinearModel<float>;
template class LinearModel<double>;


} // namespace dt
