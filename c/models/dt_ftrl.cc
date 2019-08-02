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
#include "models/dt_ftrl.h"
#include "parallel/api.h"
#include "parallel/atomic.h"
#include "utils/macros.h"
#include "wstringcol.h"
#include "column.h"
#include "progress/work.h"      // dt::progress::work


namespace dt {


/**
 *  Constructor based on the provided parameters.
 */
template <typename T>
Ftrl<T>::Ftrl(FtrlParams params_in) :
  model_type(FtrlModelType::NONE), // `NONE` means model was not trained
  params(params_in),
  alpha(static_cast<T>(params_in.alpha)),
  beta(static_cast<T>(params_in.beta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nbins(params_in.nbins),
  mantissa_nbits(params_in.mantissa_nbits),
  nepochs(params_in.nepochs),
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
 *  Constructor with default parameters.
 */
template <typename T>
Ftrl<T>::Ftrl() : Ftrl(FtrlParams()) {
}


/**
 *  Depending on a requested problem type, this method calls
 *  an appropriate `fit_*` method, and returns epoch at which
 *  training stopped, and the corresponding loss.
 */
template <typename T>
FtrlFitOutput Ftrl<T>::dispatch_fit(const DataTable* dt_X_train_in,
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
  FtrlFitOutput res;

  SType stype_y = dt_y_train->get_ocolumn(0).stype();
  FtrlModelType model_type_train = !is_model_trained()? params.model_type :
                                                        model_type;

  xassert(model_type_train != FtrlModelType::NONE);
  switch (model_type_train) {
    case FtrlModelType::AUTO :        switch (stype_y) {
                                        case SType::BOOL:    res = fit_binomial(); break;
                                        case SType::INT8:    res = fit_regression<int8_t>(); break;
                                        case SType::INT16:   res = fit_regression<int16_t>(); break;
                                        case SType::INT32:   res = fit_regression<int32_t>(); break;
                                        case SType::INT64:   res = fit_regression<int64_t>(); break;
                                        case SType::FLOAT32: res = fit_regression<float>(); break;
                                        case SType::FLOAT64: res = fit_regression<double>(); break;
                                        case SType::STR32:   FALLTHROUGH;
                                        case SType::STR64:   res = fit_multinomial(); break;
                                        default:             throw TypeError() << "Target column type `"
                                                                               << stype_y << "` is not supported";
                                      }
                                      break;

    case FtrlModelType::REGRESSION :  switch (stype_y) {
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

    case FtrlModelType::BINOMIAL :    res = fit_binomial(); break;
    case FtrlModelType::MULTINOMIAL : res = fit_multinomial(); break;
    case FtrlModelType::NONE : throw ValueError() << "Cannot train model in an unknown mode";
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
FtrlFitOutput Ftrl<T>::fit_binomial() {
  dtptr dt_y_train_binomial, dt_y_val_binomial;
  bool validation = !std::isnan(nepochs_val);
  create_y_binomial(dt_y_train, dt_y_train_binomial, label_ids_train);

  // NA values are ignored during training, so if we stop training right away,
  // if got only NA's.
  if (dt_y_train_binomial == nullptr) return {0, static_cast<double>(T_NAN)};
  dt_y_train = dt_y_train_binomial.get();

  if (validation) {
    create_y_binomial(dt_y_val, dt_y_val_binomial, label_ids_val);
    if (dt_y_val_binomial == nullptr) {
      throw ValueError() << "Cannot set early stopping criteria as validation "
                            "target column got only `NA` targets";
    }
    dt_y_val = dt_y_val_binomial.get();
  }

  if (!is_model_trained()) {
    model_type = FtrlModelType::BINOMIAL;
    create_model();
  }

  return fit<int8_t>(sigmoid<T>,
                     [] (int8_t y, size_t label_id) -> int8_t {
                       return static_cast<size_t>(y) == label_id;
                     },
                     log_loss<T>);
}


/**
 *  Convert target column to boolean type, and set up mapping
 *  between models and the incoming label inficators.
 */
template <typename T>
void Ftrl<T>::create_y_binomial(const DataTable* dt,
                                dtptr& dt_binomial,
                                std::vector<size_t>& label_ids) {
  xassert(label_ids.size() == 0);
  dtptr dt_labels_in;
  label_encode(dt->get_ocolumn(0), dt_labels_in, dt_binomial, true);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return;
  size_t nlabels_in = dt_labels_in->nrows;

  if (nlabels_in > 2) {
    throw ValueError() << "For binomial regression target column should have "
                       << "two labels at maximum, got: " << nlabels_in;
  }

  // By default we assume zero model got zero label id
  label_ids.push_back(0);

  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
  } else {

    RowIndex ri_join = natural_join(dt_labels_in.get(), dt_labels.get());
    size_t nlabels = dt_labels->nrows;
    xassert(nlabels != 0 && nlabels < 3);
    auto data_label_ids_in = static_cast<int8_t*>(dt_labels_in->get_ocolumn(1)->data_w());
    auto data_label_ids = static_cast<const int8_t*>(dt_labels->get_ocolumn(1)->data());


    switch (nlabels) {
      case 1: switch (nlabels_in) {
                 case 1: if (ri_join[0] == RowIndex::NA) {
                           // The new label we got was encoded with zeros,
                           // so we train the model on all negatives: 1 == 0
                           label_ids[0] = 1;
                           data_label_ids_in[0] = 1;
                           dt_labels->rbind({ dt_labels_in.get() }, {{ 0 }, { 1 }});
                           intvec keys{ 0 };
                           dt_labels->set_key(keys);
                         }
                         break;
                 case 2: if (ri_join[0] == RowIndex::NA && ri_join[1] == RowIndex::NA) {
                           throw ValueError() << "Got two new labels in the target column, "
                                              << "however, positive label is already set";
                         }
                         // If the new label is the zero label,
                         // then we need to train on the existing label indicator
                         // i.e. the first one.
                         label_ids[0] = static_cast<size_t>(data_label_ids_in[ri_join[0] == RowIndex::NA]);
                         // Reverse labels id order if the new label comes first.
                         if (label_ids[0] == 1) {
                           data_label_ids_in[0] = 1;
                           data_label_ids_in[1] = 0;
                         }
                         dt_labels = std::move(dt_labels_in);
              }
              break;

      case 2: switch (nlabels_in) {
                case 1: if (ri_join[0] == RowIndex::NA) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        label_ids[0] = (data_label_ids[ri_join[0]] == 1);
                        break;
                case 2: if (ri_join[0] == RowIndex::NA || ri_join[1] == RowIndex::NA) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        size_t label_id = static_cast<size_t>(data_label_ids[ri_join[0]] != 0);
                        label_ids[0] = static_cast<size_t>(data_label_ids_in[label_id]);

                        break;
              }
              break;
    }
  }
}


/**
 *  Create labels (in the case of numeric regression there is no actual
 *  labeles, so we just use a column name for this purpose),
 *  set up identity mapping between models and the incoming label inficators,
 *  call to the main `fit` method.
 */
template <typename T>
template <typename U>
FtrlFitOutput Ftrl<T>::fit_regression() {
  xassert(dt_y_train->ncols == 1);
  if (is_model_trained() && model_type != FtrlModelType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }
  if (!is_model_trained()) {
    const strvec& colnames = dt_y_train->get_names();
    std::unordered_map<std::string, int8_t> colnames_map = {{colnames[0], 0}};
    dt_labels = create_dt_labels_str<uint32_t, SType::BOOL>(colnames_map);

    create_model();
    model_type = FtrlModelType::REGRESSION;
  }
  label_ids_train = { 0 };
  label_ids_val = { 0 };

  return fit<U>(identity<T>,
                [](U y, size_t label_indicator) -> U {
                  (void) label_indicator;
                  return y;
                },
                squared_loss<T, U>);
}


/**
 *  Prepare data for multinomial problem, and call the main fit method.
 */
template <typename T>
FtrlFitOutput Ftrl<T>::fit_multinomial() {
  if (is_model_trained() && model_type != FtrlModelType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a multinomial mode this model should be reset.";
  }


  dtptr dt_y_train_multinomial;
  create_y_multinomial(dt_y_train, dt_y_train_multinomial, label_ids_train);

  if (dt_y_train_multinomial == nullptr) return {0, static_cast<double>(T_NAN)};
  dt_y_train = dt_y_train_multinomial.get();

  // Create validation targets if needed.
  dtptr dt_y_val_multinomial;
  if (!std::isnan(nepochs_val)) {
    create_y_multinomial(dt_y_val, dt_y_val_multinomial, label_ids_val, true);
    if (dt_y_val_multinomial == nullptr)
      throw ValueError() << "Cannot set early stopping criteria as validation "
                         << "target column got only `NA` targets";
    dt_y_val = dt_y_val_multinomial.get();
  }

  if (!is_model_trained()) {
    xassert(dt_model == nullptr);
    create_model();
    model_type = FtrlModelType::MULTINOMIAL;
  }

  return fit<int32_t>(sigmoid<T>,
                      [] (int32_t y, size_t label_indicator) -> int32_t {
                        return static_cast<size_t>(y) == label_indicator;
                      },
                      log_loss<T>);
}


/**
 *  Encode target column with the integer labels, and set up mapping
 *  between models and the incoming label indicators.
 */
template <typename T>
void Ftrl<T>::create_y_multinomial(const DataTable* dt,
                                      dtptr& dt_multinomial,
                                      std::vector<size_t>& label_ids,
                                      bool validation /* = false */) {
  xassert(label_ids.size() == 0)
  dtptr dt_labels_in;
  label_encode(dt->get_ocolumn(0), dt_labels_in, dt_multinomial);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return;

  auto data_label_ids_in = static_cast<const int32_t*>(dt_labels_in->get_ocolumn(1)->data());
  size_t nlabels_in = dt_labels_in->nrows;

  // When we only start training, all the incoming labels become the model
  // labels. Mapping is trivial in this case.
  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
    label_ids.resize(nlabels_in);
    for (size_t i = 0; i < nlabels_in; ++i) {
      label_ids[i] = i;
    }

  } else {
    // When we already have some labels, and got new ones, we first
    // set up mapping in such a way, so that models will train
    // on all the negatives.
    auto data_label_ids = static_cast<const int32_t*>(dt_labels->get_ocolumn(1)->data());
    RowIndex ri_join = natural_join(dt_labels_in.get(), dt_labels.get());
    size_t nlabels = dt_labels->nrows;

    for (size_t i = 0; i < nlabels; ++i) {
      label_ids.push_back(std::numeric_limits<size_t>::max());
    }

    // Then we go through the list of new labels and relate existing models
    // to the incoming label indicators.
    arr64_t new_label_indices(nlabels_in);
    int64_t* data = new_label_indices.data();
    size_t n_new_labels = 0;
    for (size_t i = 0; i < nlabels_in; ++i) {
      size_t ri = ri_join[i];
      size_t label_id_in = static_cast<size_t>(data_label_ids_in[i]);
      if (ri != RowIndex::NA) {
        size_t label_id = static_cast<size_t>(data_label_ids[ri]);
        label_ids[label_id] = label_id_in;
      } else {
        // If there is no corresponding label already set,
        // we will need to create a new one and its  model.
        data[n_new_labels] = static_cast<int64_t>(i);
        label_ids.push_back(label_id_in);
        n_new_labels++;
      }
    }

    if (n_new_labels) {
      // In the case of validation we don't allow unseen labels.
      if (validation) {
        throw ValueError() << "Validation target column cannot contain labels, "
                           << "the model was not trained on";
      }

      // Extract new labels from `dt_labels_in`, and rbind them to `dt_labels`.
      new_label_indices.resize(n_new_labels);
      RowIndex ri_labels(std::move(new_label_indices));
      dt_labels_in->apply_rowindex(ri_labels);
      set_ids(dt_labels_in->get_ocolumn(1), static_cast<int32_t>(dt_labels->nrows));
      dt_labels->rbind({ dt_labels_in.get() }, {{ 0 } , { 1 }});

      // It is necessary to re-key the column, because there is no guarantee
      // that rbind didn't break data ordering.
      intvec keys{ 0 };
      dt_labels->set_key(keys);

      // Add new models for the new labels.
      if (n_new_labels) adjust_model();
    }

  }
}


/**
 *  Fit model on a datatable.
 */
template <typename T>
template <typename U> /* target column(s) data type */
FtrlFitOutput Ftrl<T>::fit(T(*linkfn)(T), U(*targetfn)(U, size_t), T(*lossfn)(T, U)) {
  // Define features, weight pointers, feature importances storage,
  // as well as column hashers.
  define_features();
  init_weights();
  if (dt_fi == nullptr) create_fi();
  auto hashers = create_hashers(dt_X_train);

  // Obtain rowindex and data pointers for the target column(s).
  std::vector<RowIndex> ri, ri_val;
  std::vector<const U*> data_y, data_y_val;
  fill_ri_data<U>(dt_y_train, ri, data_y);
  auto data_fi = static_cast<T*>(dt_fi->get_ocolumn(1)->data_w());

  // Training settings. By default each training iteration consists of
  // `dt_X_train->nrows` rows.
  size_t niterations = nepochs;
  size_t iteration_nrows = dt_X_train->nrows;
  size_t total_nrows = niterations * iteration_nrows;

  // If a validation set is provided, we adjust batch size to `nepochs_val`.
  // After each batch, we calculate loss on the validation dataset,
  // and do early stopping if relative loss does not decrese by at least
  // `val_error`.
  bool validation = !std::isnan(nepochs_val);
  T loss = T_NAN; // This value is returned when validation is not enabled
  T loss_old = T_ZERO; // Value of `loss` for a previous iteraction
  std::vector<T> loss_history;
  std::vector<hasherptr> hashers_val;
  if (validation) {
    hashers_val = create_hashers(dt_X_val);
    iteration_nrows = static_cast<size_t>(nepochs_val * dt_X_train->nrows);
    niterations = total_nrows / iteration_nrows;
    loss_history.resize(val_niters, 0.0);
    fill_ri_data<U>(dt_y_val, ri_val, data_y_val);
  }


  std::mutex m;
  size_t iteration_end = 0;

  // If we request more threads than is available, `dt::parallel_region()`
  // will fall back to the possible maximum.
  size_t nthreads = get_nthreads(iteration_nrows);
  size_t iteration_nrows_per_thread = iteration_nrows / nthreads;
  size_t total_work_amount = total_nrows / nthreads;

  dt::progress::work job(total_work_amount);
  job.set_message("Fitting");

  dt::parallel_region(nthreads,
    [&]() {
      // Each thread gets a private storage for hashes,
      // temporary weights and feature importances.
      uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
      tptr<T> w = tptr<T>(new T[nfeatures]);
      tptr<T> fi = tptr<T>(new T[nfeatures]());

      for (size_t iter = 0; iter < niterations; ++iter) {
        size_t iteration_start = iter * iteration_nrows;
        iteration_end = (iter == niterations - 1)? total_nrows :
                                                   (iter + 1) * iteration_nrows;
        size_t iteration_size = iteration_end - iteration_start;

        // Training.
        dt::nested_for_static(iteration_size, [&](size_t i) {
          size_t ii = (iteration_start + i) % dt_X_train->nrows;
          const size_t j0 = ri[0][ii];

          if (j0 != RowIndex::NA && !ISNA<U>(data_y[0][j0])) {
            hash_row(x, hashers, ii);
            for (size_t k = 0; k < label_ids_train.size(); ++k) {
              const size_t j = ri[0][ii];

              T p = linkfn(predict_row(
                      x, w, k,
                      [&](size_t f_id, T f_imp) {
                        fi[f_id] += f_imp;
                      }
                    ));

              // `targetfn` returns the actual target for the k-th model,
              // depending on the data in the y column and the label indicator.
              // When we do multilabel, we will have a set of y columns
              // and some sort of loop over the data_y[i] here.
              U y = targetfn(data_y[0][j], label_ids_train[k]);
              update(x, w, p, y, k);
            }
          }

          // Report progress
          if (dt::this_thread_index() == 0) {
            job.set_done_amount(iter * iteration_nrows_per_thread + i);
          }

        }); // End training.
        barrier();

        // Validation and early stopping.
        if (validation) {
          dt::atomic<T> loss_global {0.0};
          T loss_local = 0.0;

          dt::nested_for_static(dt_X_val->nrows, [&](size_t i) {
            const size_t j0 = ri_val[0][i];

            if (j0 != RowIndex::NA && !ISNA<U>(data_y_val[0][j0])) {
              hash_row(x, hashers_val, i);
              for (size_t k = 0; k < label_ids_val.size(); ++k) {
                const size_t j = ri_val[0][i];
                T p = linkfn(predict_row(
                        x, w, k, [&](size_t, T){}
                      ));
                U y = targetfn(data_y_val[0][j], label_ids_val[k]);
                loss_local += lossfn(p, y);
              }
            }
          });
          loss_global.fetch_add(loss_local);
          barrier();

          // Thread #0 checks relative loss change and, if it does not decrease
          // more than `val_error`, sets `loss_old` to `NaN` -> this will stop
          // all the threads after `barrier()`.
          if (dt::this_thread_index() == 0) {
            T loss_current = loss_global.load() / (dt_X_val->nrows * dt_y_val->ncols);
            loss_history[iter % val_niters] = loss_current / val_niters;
            loss = std::accumulate(loss_history.begin(), loss_history.end(), T_ZERO);
            T loss_diff = (loss_old - loss) / loss_old;
            bool is_loss_bad = (iter >= val_niters) && (loss < T_EPSILON || loss_diff < val_error);
            loss_old = is_loss_bad? T_NAN : loss;
          }
          barrier();

          if (std::isnan(loss_old)) {
            if (dt::this_thread_index() == 0) {
              job.set_message("Fitting: early stopping criteria is met");
            }
            break;
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


  double epoch_stopped = static_cast<double>(iteration_end) / dt_X_train->nrows;
  FtrlFitOutput res = {epoch_stopped, static_cast<double>(loss)};
  job.set_done_amount(total_work_amount);
  job.done();

  return res;
}


/**
 *  Make a prediction for an array of hashed features.
 */
template <typename T>
template <typename F>
T Ftrl<T>::predict_row(const uint64ptr& x, tptr<T>& w, size_t k, F fifn) {
  T zero = static_cast<T>(0.0);
  T wTx = zero;
  T ia = 1 / alpha;
  T rr = beta * ia + lambda2;
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T absw = std::max(std::abs(z[k][j]) - lambda1, zero) /
             (std::sqrt(n[k][j]) * ia + rr);
    w[i] = -std::copysign(absw, z[k][j]);
    wTx += w[i];
    fifn(i, absw);
  }
  return wTx;
}


/**
 *  Update weights based on a prediction p and the actual target y.
 */
template <typename T>
template <typename U /* column data type */>
void Ftrl<T>::update(const uint64ptr& x, const tptr<T>& w,
                         T p, U y, size_t k) {
  T ia = 1 / alpha;
  T g = p - static_cast<T>(y);
  T gsq = g * g;
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T sigma = (std::sqrt(n[k][j] + gsq) - std::sqrt(n[k][j])) * ia;
    z[k][j] += g - sigma * w[i];
    n[k][j] += gsq;
  }
}


/**
 *  This method calls `predict` with the proper label id type:
 *  - for binomial and numeric regression label ids are `int8`;
 *  - for multinomial regression label ids are `int32`.
 */
template <typename T>
dtptr Ftrl<T>::dispatch_predict(const DataTable* dt_X) {
  if (!is_model_trained()) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }

  SType label_id_stype = dt_labels->get_ocolumn(1).stype();
  dtptr dt_p;
  switch (label_id_stype) {
    case SType::BOOL:  dt_p = predict<int8_t>(dt_X); break;
    case SType::INT32: dt_p = predict<int32_t>(dt_X); break;
    default: throw TypeError() << "Label id type  `"
                               << label_id_stype << "` is not supported";
  }

  return dt_p;
}


/**
 *  Predict on a datatable and return a new datatable with
 *  the predicted probabilities.
 */
template <typename T>
template <typename U /* label id type */>
dtptr Ftrl<T>::predict(const DataTable* dt_X) {
  if (!is_model_trained()) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }

  // Re-acquire model weight pointers.
  init_weights();

  // Re-create hashers, as stypes for predictions may be different.
  auto hashers = create_hashers(dt_X);

  // Create datatable for predictions and obtain column data pointers.
  size_t nlabels = dt_labels->nrows;

  auto data_label_ids = static_cast<const U*>(dt_labels->get_ocolumn(1)->data());

  dtptr dt_p = create_p(dt_X->nrows);
  std::vector<T*> data_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    data_p[i] = static_cast<T*>(dt_p->get_ocolumn(i)->data_w());
  }

  // Determine which link function we should use.
  T (*linkfn)(T);
  switch (model_type) {
    case FtrlModelType::REGRESSION  : linkfn = identity<T>; break;
    case FtrlModelType::BINOMIAL    : linkfn = sigmoid<T>; break;
    case FtrlModelType::MULTINOMIAL : (nlabels < 3)? linkfn = sigmoid<T> :
                                                     linkfn = std::exp;
                                      break;
    default : throw ValueError() << "Cannot do any predictions, "
                                 << "the model was trained in an unknown mode";
  }

  size_t nthreads = get_nthreads(dt_X->nrows);
  nthreads = std::min(std::max(nthreads, 1lu), dt::num_threads_in_pool());
  bool k_binomial;
  size_t total_work_amount = dt_X->nrows / nthreads;

  dt::progress::work job(total_work_amount);
  job.set_message("Predicting");
  dt::parallel_region(nthreads, [&]() {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    tptr<T> w = tptr<T>(new T[nfeatures]);

    dt::nested_for_static(dt_X->nrows, [&](size_t i){
      hash_row(x, hashers, i);
      for (size_t k = 0; k < nlabels; ++k) {
        size_t label_id = static_cast<size_t>(data_label_ids[k]);
        if (model_type == FtrlModelType::BINOMIAL && label_id == 1) {
          k_binomial = k;
          continue;
        }

        data_p[k][i] = linkfn(predict_row(x, w, label_id, [&](size_t, T){}));
      }
      if (dt::this_thread_index() == 0) {
        job.set_done_amount(i);
      }
    });
  });
  job.set_done_amount(total_work_amount);
  job.done();

  if (model_type == FtrlModelType::BINOMIAL) {
    dt::parallel_for_static(dt_X->nrows, [&](size_t i){
      data_p[k_binomial][i] = T_ONE - data_p[!k_binomial][i];
    });
  }

  // For multinomial case, when there is two labels, we match binomial
  // classifier by using `sigmoid` link function. When there is more
  // than two labels, we use `std::exp` link function, and do normalization,
  // so that predictions sum up to 1, effectively doing `softmax` linking.
  if (nlabels > 2) normalize_rows(dt_p);
  return dt_p;
}


/**
 *  Obtain pointers to column rowindexes and data.
 */
template <typename T>
template <typename U /* column data type */>
void Ftrl<T>::fill_ri_data(const DataTable* dt,
                           std::vector<RowIndex>& ri,
                           std::vector<const U*>& data) {
  size_t ncols = dt->ncols;
  ri.reserve(ncols);
  data.reserve(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    const OColumn& col = dt->get_ocolumn(i);
    data.push_back(static_cast<const U*>(col->data()));
    ri.push_back(col->rowindex());
  }
}


/**
 *  Normalize rows in a datatable, so that their values sum up to 1.
 */
template <typename T>
void Ftrl<T>::normalize_rows(dtptr& dt) {
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;

  std::vector<T*> data(ncols);
  for (size_t j = 0; j < ncols; ++j) {
    data[j] = static_cast<T*>(dt->get_ocolumn(j)->data_w());
  }

  dt::parallel_for_static(nrows, [&](size_t i){
    T sum = static_cast<T>(0.0);
    for (size_t j = 0; j < ncols; ++j) {
      sum += data[j][i];
    }
    for (size_t j = 0; j < ncols; ++j) {
      data[j][i] /= sum;
    }
  });
}


/**
 *  Create model datatable of shape (nbins, 2 * nlabels) to store z and n
 *  coefficients.
 */
template <typename T>
void Ftrl<T>::create_model() {
  size_t nlabels = (dt_labels == nullptr)? 0 : dt_labels->nrows;
  size_t ncols = (model_type == FtrlModelType::BINOMIAL)? 2 : 2 * nlabels;

  colvec cols;
  cols.reserve(ncols);
  constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
  for (size_t i = 0; i < ncols; ++i) {
    cols.push_back(OColumn::new_data_column(stype, nbins));
  }
  dt_model = dtptr(new DataTable(std::move(cols), DataTable::default_names));
  init_model();
}


/**
 *  This method is invoked in the case when we get new labels
 *  for multinomial classification and need to add them to the model.
 *  In such a case, we make a copy of the "negative" z and n
 *  coefficients adding them to the existing `dt_model` columns.
 */
template <typename T>
void Ftrl<T>::adjust_model() {
  size_t ncols_model = dt_model->ncols;
  size_t ncols_model_new = 2 * dt_labels->nrows;
  xassert(ncols_model_new > ncols_model)

  colvec cols;
  cols.reserve(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols.push_back(dt_model->get_ocolumn(i));
  }

  OColumn newcol0, newcol1;
  // If we have a negative class, then all the new classes
  // get a copy of its weights to start learning from.
  // Otherwise, new classes start learning from zero weights.
  // if (params.negative_class) {
  //   newcol0 = dt_model->get_ocolumn(0);
  //   cols_new[1] = dt_model->get_ocolumn(1);
  // } else
  {
    constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
    OColumn col = OColumn::new_data_column(stype, nbins);
    auto data = static_cast<T*>(col->data_w());
    std::memset(data, 0, nbins * sizeof(T));
    newcol0 = col;
    newcol1 = col;
  }


  for (size_t i = ncols_model; i < ncols_model_new; i+=2) {
    cols.push_back(newcol0);
    cols.push_back(newcol1);
  }

  dt_model = dtptr(new DataTable(std::move(cols), DataTable::default_names));
}


/**
 *  Create datatable for predictions.
 */
template <typename T>
dtptr Ftrl<T>::create_p(size_t nrows) {
  size_t nlabels = dt_labels->nrows;
  xassert(nlabels > 0);

  OColumn col0_str64 = dt_labels->get_ocolumn(0).cast(SType::STR64);

  strvec labels_vec(nlabels);

  for (size_t i = 0; i < nlabels; ++i) {
    CString value;
    bool isna = col0_str64.get_element(i, &value);
    labels_vec[i] = isna? std::string()
                        : std::string(value.ch, static_cast<size_t>(value.size));
  }

  colvec cols;
  cols.reserve(nlabels);
  constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
  for (size_t i = 0; i < nlabels; ++i) {
    cols.push_back(OColumn::new_data_column(stype, nrows));
  }

  // dtptr dt_p = dtptr(new DataTable(std::move(cols), labels));
  dtptr dt_p = dtptr(new DataTable(std::move(cols), std::move(labels_vec)));
  return dt_p;
}


/**
 *  Reset the model.
 */
template <typename T>
void Ftrl<T>::reset() {
  dt_model = nullptr;
  dt_fi = nullptr;
  model_type = FtrlModelType::NONE;
  dt_labels = nullptr;
  colname_hashes.clear();
}


/**
 *  Initialize model coefficients with zeros.
 */
template <typename T>
void Ftrl<T>::init_model() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols; ++i) {
    auto data = static_cast<T*>(dt_model->get_ocolumn(i)->data_w());
    std::memset(data, 0, nbins * sizeof(T));
  }
}


/**
 *  Obtain pointers to the model column data.
 */
template <typename T>
void Ftrl<T>::init_weights() {
  size_t model_ncols = dt_model->ncols;
  xassert(model_ncols % 2 == 0);
  size_t nlabels = model_ncols / 2;
  z.clear();
  z.reserve(nlabels);
  n.clear();
  n.reserve(nlabels);

  for (size_t k = 0; k < nlabels; ++k) {
    z.push_back(static_cast<T*>(dt_model->get_ocolumn(2 * k)->data_w()));
    n.push_back(static_cast<T*>(dt_model->get_ocolumn(2 * k + 1)->data_w()));
  }
}


/**
 * Create feature importance datatable.
 */
template <typename T>
void Ftrl<T>::create_fi() {
  const strvec& colnames = dt_X_train->get_names();

  dt::writable_string_col c_fi_names(nfeatures);
  dt::writable_string_col::buffer_impl<uint32_t> sb(c_fi_names);
  sb.commit_and_start_new_chunk(0);
  for (const auto& feature_name : colnames) {
    sb.write(feature_name);
  }

  if (interactions.size()) {
    for (auto interaction : interactions) {
      std::string feature_interaction;
      for (auto feature_id : interaction) {
        feature_interaction += colnames[feature_id] + ":";
      }
      feature_interaction.pop_back();
      sb.write(feature_interaction);
    }
  }

  sb.order();
  sb.commit_and_start_new_chunk(nfeatures);

  constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
  OColumn c_fi_values = OColumn::new_data_column(stype, nfeatures);
  dt_fi = dtptr(new DataTable({std::move(c_fi_names).to_ocolumn(), std::move(c_fi_values)},
                              {"feature_name", "feature_importance"})
                             );
  init_fi();
}


/**
 *  Initialize feature importances with zeros.
 */
template <typename T>
void Ftrl<T>::init_fi() {
  if (dt_fi == nullptr) return;
  auto data = static_cast<T*>(dt_fi->get_ocolumn(1)->data_w());
  std::memset(data, 0, nfeatures * sizeof(T));
}


/**
 *  Determine number of features.
 */
template <typename T>
void Ftrl<T>::define_features() {
  nfeatures = dt_X_train->ncols + interactions.size();
}


/**
 *  Create hashers for all datatable column.
 */
template <typename T>
std::vector<hasherptr> Ftrl<T>::create_hashers(const DataTable* dt) {
  std::vector<hasherptr> hashers;
  hashers.clear();
  hashers.reserve(dt->ncols);

  // Create hashers.
  for (size_t i = 0; i < dt->ncols; ++i) {
    const OColumn& col = dt->get_ocolumn(i);
    hashers.push_back(create_hasher(col));
  }

  // Hash column names.
  const std::vector<std::string>& c_names = dt->get_names();
  colname_hashes.clear();
  colname_hashes.reserve(dt->ncols);
  for (size_t i = 0; i < dt->ncols; i++) {
    uint64_t h = hash_murmur2(c_names[i].c_str(),
                             c_names[i].length() * sizeof(char));
    colname_hashes.push_back(h);
  }

  return hashers;
}


/**
 *  Depending on a column type, create a corresponding hasher.
 */
template <typename T>
hasherptr Ftrl<T>::create_hasher(const OColumn& col) {
  int shift_nbits = dt::FtrlBase::DOUBLE_MANTISSA_NBITS - mantissa_nbits;
  switch (col.stype()) {
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:   return hasherptr(new HasherInt<int32_t>(col));
    case SType::INT64:   return hasherptr(new HasherInt<int64_t>(col));
    case SType::FLOAT32: return hasherptr(new HasherFloat<float>(col, shift_nbits));
    case SType::FLOAT64: return hasherptr(new HasherFloat<double>(col, shift_nbits));
    case SType::STR32:
    case SType::STR64:   return hasherptr(new HasherString(col));
    default:             throw  TypeError() << "Cannot hash a column of type "
                                            << col.stype();
  }
}


/**
 *  Hash each element of the datatable row, do feature interactions if requested.
 */
template <typename T>
void Ftrl<T>::hash_row(uint64ptr& x, std::vector<hasherptr>& hashers,
                           size_t row) {
  // Hash column values adding a column name hash, so that the same value
  // in different columns results in different hashes.
  for (size_t i = 0; i < hashers.size(); ++i) {
    x[i] = (hashers[i]->hash(row) + colname_hashes[i]) % nbins;
  }

  // Do feature interactions.
  if (interactions.size() > 0) {
    size_t count = 0;
    for (auto interaction : interactions) {
      size_t i = hashers.size() + count;
      x[i] = 0;
      for (auto feature_id : interaction) {
        x[i] += x[feature_id];
      }
      x[i] %= nbins;
      count++;
    }
  }
}


/**
 *  Return training status.
 */
template <typename T>
bool Ftrl<T>::is_model_trained() {
  return model_type != FtrlModelType::NONE;
}


/**
 *  Get a shallow copy of a model if available.
 */
template <typename T>
DataTable* Ftrl<T>::get_model() {
  if (dt_model == nullptr) return nullptr;
  return dt_model->copy();
}


/**
 *  Return model type.
 */
template <typename T>
FtrlModelType Ftrl<T>::get_model_type() {
  return params.model_type;
}


/**
 *  Return trained model type.
 */
template <typename T>
FtrlModelType Ftrl<T>::get_model_type_trained() {
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
DataTable* Ftrl<T>::get_fi(bool normalize /* = true */) {
  if (dt_fi == nullptr) return nullptr;

  DataTable* dt_fi_copy = dt_fi->copy();
  if (normalize) {
    OColumn& ocol = dt_fi_copy->get_ocolumn(1);
    auto col = static_cast<RealColumn<T>*>(const_cast<Column*>(ocol.get()));
    T max = col->max();
    T* data = col->elements_w();
    T norm_factor = static_cast<T>(1.0);

    if (fabs(max) > T_EPSILON) norm_factor /= max;
    for (size_t i = 0; i < col->nrows; ++i) {
      data[i] *= norm_factor;
    }
    col->get_stats()->reset();
  }
  return dt_fi_copy;
}


/**
 *  Other getters and setters.
 *  Here we assume that all the validation for setters is done by py::Ftrl.
 */
template <typename T>
const std::vector<uint64_t>& Ftrl<T>::get_colname_hashes() {
  return colname_hashes;
}


template <typename T>
size_t Ftrl<T>::get_ncols() {
  return colname_hashes.size();
}


template <typename T>
size_t Ftrl<T>::get_nfeatures() {
  return nfeatures;
}


template <typename T>
double Ftrl<T>::get_alpha() {
  return params.alpha;
}


template <typename T>
double Ftrl<T>::get_beta() {
  return params.beta;
}


template <typename T>
double Ftrl<T>::get_lambda1() {
  return params.lambda1;
}


template <typename T>
double Ftrl<T>::get_lambda2() {
  return params.lambda2;
}


template <typename T>
uint64_t Ftrl<T>::get_nbins() {
  return params.nbins;
}


template <typename T>
unsigned char Ftrl<T>::get_mantissa_nbits() {
  return params.mantissa_nbits;
}


template <typename T>
const std::vector<intvec>& Ftrl<T>::get_interactions() {
  return interactions;
}


template <typename T>
size_t Ftrl<T>::get_nepochs() {
  return params.nepochs;
}


template <typename T>
bool Ftrl<T>::get_negative_class() {
  return params.negative_class;
}


template <typename T>
FtrlParams Ftrl<T>::get_params() {
  return params;
}


template <typename T>
DataTable* Ftrl<T>::get_labels() {
  if (dt_labels == nullptr) return nullptr;
  DataTable* dt_labels_copy = dt_labels->copy();
  return dt_labels_copy;
}


template <typename T>
void Ftrl<T>::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_nbins(dt_model->nrows);
  nfeatures = 0;
}


template <typename T>
void Ftrl<T>::set_model_type(FtrlModelType model_type_in) {
  params.model_type = model_type_in;
}


template <typename T>
void Ftrl<T>::set_model_type_trained(FtrlModelType model_type_trained_in) {
  model_type = model_type_trained_in;
}


template <typename T>
void Ftrl<T>::set_fi(DataTable* dt_fi_in) {
  dt_fi = dtptr(dt_fi_in->copy());
  nfeatures = dt_fi->nrows;
}


template <typename T>
void Ftrl<T>::set_alpha(double alpha_in) {
  params.alpha = alpha_in;
  alpha = static_cast<T>(alpha_in);
}


template <typename T>
void Ftrl<T>::set_beta(double beta_in) {
  params.beta = beta_in;
  beta = static_cast<T>(beta_in);
}


template <typename T>
void Ftrl<T>::set_lambda1(double lambda1_in) {
  params.lambda1 = lambda1_in;
  lambda1 = static_cast<T>(lambda1_in);
}


template <typename T>
void Ftrl<T>::set_lambda2(double lambda2_in) {
  params.lambda2 = lambda2_in;
  lambda2 = static_cast<T>(lambda2_in);
}


template <typename T>
void Ftrl<T>::set_nbins(uint64_t nbins_in) {
  params.nbins = nbins_in;
  nbins = nbins_in;
}


template <typename T>
void Ftrl<T>::set_mantissa_nbits(unsigned char mantissa_nbits_in) {
  xassert(mantissa_nbits_in >= 0);
  xassert(mantissa_nbits_in <= dt::FtrlBase::DOUBLE_MANTISSA_NBITS);
  params.mantissa_nbits = mantissa_nbits_in;
  mantissa_nbits = mantissa_nbits_in;
}


template <typename T>
void Ftrl<T>::set_interactions(std::vector<intvec> interactions_in) {
  interactions = std::move(interactions_in);
}


template <typename T>
void Ftrl<T>::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
  nepochs = nepochs_in;
}


template <typename T>
void Ftrl<T>::set_negative_class(bool negative_class_in) {
  params.negative_class = negative_class_in;
}


template <typename T>
void Ftrl<T>::set_labels(DataTable* dt_labels_in) {
  dt_labels = dtptr(dt_labels_in->copy());
}


template class Ftrl<float>;
template class Ftrl<double>;


} // namespace dt
