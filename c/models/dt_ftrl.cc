//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "frame/py_frame.h"
#include "models/dt_ftrl.h"
#include "parallel/api.h"
#include "parallel/atomic.h"
#include "progress/work.h"      // dt::progress::work
#include "utils/macros.h"
#include "column.h"
#include "wstringcol.h"

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
  nfeatures(0),
  dt_X_train(nullptr),
  dt_y_train(nullptr),
  dt_X_val(nullptr),
  dt_y_val(nullptr),
  nepochs_val(T_NAN),
  val_error(T_NAN),
  val_niters(0)
{}


/**
 *  Constructor with default parameters.
 */
template <typename T>
Ftrl<T>::Ftrl() : Ftrl(FtrlParams()) {}


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

  SType stype_y = dt_y_train->get_column(0).stype();
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
                                        case SType::STR32:
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

  // For binomial regression training and validation target columns
  // are both SType::BOOL, see `label_encode()` implementation for more
  // details.
  auto targetfn = [] (int8_t y, size_t label_id) -> int8_t {
                       return static_cast<size_t>(y) == label_id;
                     };
  return fit<int8_t, int8_t>(sigmoid<T>, targetfn, targetfn, log_loss<T>);
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
  label_encode(dt->get_column(0), dt_labels_in, dt_binomial, true);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return;
  size_t nlabels_in = dt_labels_in->nrows();

  if (nlabels_in > 2) {
    throw ValueError() << "For binomial regression target column should have "
                       << "two labels at maximum, got: " << nlabels_in;
  }

  // By default we assume zero model got zero label id
  label_ids.push_back(0);

  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
  } else {

    RowIndex ri_join = natural_join(*dt_labels_in.get(), *dt_labels.get());
    size_t nlabels = dt_labels->nrows();
    xassert(nlabels != 0 && nlabels < 3);
    auto data_label_ids_in = static_cast<int32_t*>(
                              dt_labels_in->get_column(1).get_data_editable());
    auto data_label_ids = static_cast<const int32_t*>(
                              dt_labels->get_column(1).get_data_readonly());

    size_t ri0_index = 0, ri1_index;
    bool ri0_valid = (ri_join.size() >= 1) && ri_join.get_element(0, &ri0_index);
    bool ri1_valid = (ri_join.size() >= 2) && ri_join.get_element(1, &ri1_index);

    switch (nlabels) {
      case 1: switch (nlabels_in) {
                 case 1: if (!ri0_valid) {
                           // The new label we got was encoded with zeros,
                           // so we train the model on all negatives: 1 == 0
                           label_ids[0] = 1;
                           data_label_ids_in[0] = 1;
                           // Since we cannot rbind anything to a keyed frame, we
                           // - clear the key;
                           // - rbind new labels;
                           // - set the key back, this will sort the resulting `dt_labels`.
                           dt_labels->clear_key();
                           dt_labels->rbind({ dt_labels_in.get() }, {{ 0 }, { 1 }});
                           intvec keys{ 0 };
                           dt_labels->set_key(keys);
                         }
                         break;
                 case 2: if (!ri0_valid && !ri1_valid) {
                           throw ValueError() << "Got two new labels in the target column, "
                                              << "however, positive label is already set";
                         }
                         // If the new label is the zero label,
                         // then we need to train on the existing label indicator
                         // i.e. the first one.
                         label_ids[0] = static_cast<size_t>(data_label_ids_in[!ri0_valid]);
                         // Reverse labels id order if the new label comes first.
                         if (label_ids[0] == 1) {
                           data_label_ids_in[0] = 1;
                           data_label_ids_in[1] = 0;
                         }
                         dt_labels = std::move(dt_labels_in);
              }
              break;

      case 2: switch (nlabels_in) {
                case 1: if (!ri0_valid) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        label_ids[0] = (data_label_ids[ri0_index] == 1);
                        break;
                case 2: if (!ri0_valid || !ri1_valid) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        size_t label_id = static_cast<size_t>(data_label_ids[ri0_index] != 0);
                        label_ids[0] = static_cast<size_t>(data_label_ids_in[label_id]);

                        break;
              }
              break;
    }
  }
}


/**
 *  Identity target to be used by `fit_regression()`.
 *  Since C++11 doesn't support templated lambdas, this one is implemented
 *  as a separate inlined function.
 */
template <typename U>
inline U itarget(U y, size_t label_indicator) {
  (void) label_indicator;
  return y;
};


/**
 *  Create labels (in the case of numeric regression there is no actual
 *  labeles, so we just use a column name for this purpose),
 *  set up identity mapping between models and the incoming label inficators,
 *  call to the main `fit` method.
 */
template <typename T>
template <typename U>
FtrlFitOutput Ftrl<T>::fit_regression() {
  xassert(dt_y_train->ncols() == 1);
  if (is_model_trained() && model_type != FtrlModelType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }

  if (!is_model_trained()) {
    const strvec& colnames = dt_y_train->get_names();
    std::unordered_map<std::string, int32_t> colnames_map = {{colnames[0], 0}};
    dt_labels = create_dt_labels_str<uint32_t>(colnames_map);

    create_model();
    model_type = FtrlModelType::REGRESSION;
  }
  label_ids_train = { 0 };
  label_ids_val = { 0 };

  FtrlFitOutput res;

  if (!std::isnan(nepochs_val)) {
    // If we got validation datasets, figure out stype of
    // the validation target column and make an appropriate call to `.fit()`.
    SType stype_y_val = dt_y_val->get_column(0).stype();
    switch (stype_y_val) {
      case SType::BOOL:    res = fit<U, int8_t>(identity<T>, itarget<U>, itarget<int8_t>, squared_loss<T, int8_t>); break;
      case SType::INT8:    res = fit<U, int8_t>(identity<T>, itarget<U>, itarget<int8_t>, squared_loss<T, int8_t>); break;
      case SType::INT16:   res = fit<U, int16_t>(identity<T>, itarget<U>, itarget<int16_t>, squared_loss<T, int16_t>); break;
      case SType::INT32:   res = fit<U, int32_t>(identity<T>, itarget<U>, itarget<int32_t>, squared_loss<T, int32_t>); break;
      case SType::INT64:   res = fit<U, int64_t>(identity<T>, itarget<U>, itarget<int64_t>, squared_loss<T, int64_t>); break;
      case SType::FLOAT32: res = fit<U, float>(identity<T>, itarget<U>, itarget<float>, squared_loss<T, float>); break;
      case SType::FLOAT64: res = fit<U, double>(identity<T>, itarget<U>, itarget<double>, squared_loss<T, double>); break;
      default:             throw TypeError() << "Target column type `"
                                             << stype_y_val << "` is not supported by numeric regression";
    }
  } else {
    // If no validation was requested, it doesn't matter
    // what validation type we are passing to the `fit()` method.
    res = fit<U, U>(identity<T>, itarget<U>, itarget<U>, squared_loss<T, U>);
  }

  return res;
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

  // For binomial regression training and validation target columns
  // are both SType::INT32, see `label_encode()` implementation for more
  // details.
  auto targetfn = [] (int32_t y, size_t label_indicator) -> int32_t {
                       return static_cast<size_t>(y) == label_indicator;
                     };
  return fit<int32_t, int32_t>(sigmoid<T>, targetfn, targetfn, log_loss<T>);
}


/**
 *  Add negative class label and save its model id.
 */
template <typename T>
void Ftrl<T>::add_negative_class() {
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
  intvec keys{ 0 };
  dt_labels->set_key(keys);
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
  label_encode(dt->get_column(0), dt_labels_in, dt_multinomial);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return;

  auto data_label_ids_in = static_cast<const int32_t*>(
                              dt_labels_in->get_column(1).get_data_readonly()
                           );
  size_t nlabels_in = dt_labels_in->nrows();

  // When we start training for the first time, all the incoming labels
  // become the model labels. Mapping is trivial in this case.
  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
    if (params.negative_class) {
      add_negative_class();
      ++nlabels_in;
    }

    label_ids.resize(nlabels_in);
    for (size_t i = 0; i < nlabels_in; ++i) {
      label_ids[i] = i - params.negative_class;
    }

  } else {
    // When we already have some labels, and got new ones, we first
    // set up mapping in such a way, so that models will train
    // on all the negatives.
    auto data_label_ids = static_cast<const int32_t*>(
                            dt_labels->get_column(1).get_data_readonly()
                          );

    RowIndex ri_join = natural_join(*dt_labels_in.get(), *dt_labels.get());
    size_t nlabels = dt_labels->nrows();

    for (size_t i = 0; i < nlabels; ++i) {
      label_ids.push_back(std::numeric_limits<size_t>::max());
    }

    // Then we go through the list of new labels and relate existing models
    // to the incoming label indicators.
    arr64_t new_label_indices(nlabels_in);
    int64_t* data = new_label_indices.data();
    size_t n_new_labels = 0;
    for (size_t i = 0; i < nlabels_in; ++i) {
      size_t rii;
      bool rii_valid = ri_join.get_element(i, &rii);
      size_t label_id_in = static_cast<size_t>(data_label_ids_in[i]);
      if (rii_valid) {
        size_t label_id = static_cast<size_t>(data_label_ids[rii]);
        label_ids[label_id] = label_id_in;
      } else {
        // If there is no corresponding label already set,
        // we will need to create a new one and its model.
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

      // Set new ids for the incoming labels, so that they can be rbinded
      // to the existing ones. NB: this operation won't affect the relation
      // between the models and label indicators, because at this point
      // it has been already set in `label_ids`.
      adjust_values<int32_t>(
        dt_labels_in->get_column(1),
        [&] (int32_t& value, size_t irow) {
          value = static_cast<int32_t>(irow + dt_labels->nrows());
        }
      );

      // Since we cannot rbind anything to a keyed frame, we
      // - clear the key;
      // - rbind new labels;
      // - set the key back, this will sort the resulting `dt_labels`.
      dt_labels->clear_key();
      dt_labels->rbind({ dt_labels_in.get() }, {{ 0 } , { 1 }});
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
template <typename U, typename V> /* target column(s) data type */
FtrlFitOutput Ftrl<T>::fit(T(*linkfn)(T),
                           U(*targetfn)(U, size_t),
                           V(*targetfn_val)(V, size_t),
                           T(*lossfn)(T, V))
{
  // Initialize helper parameters for prediction formula.
  init_helper_params();

  // Define features, weight pointers, feature importances storage,
  // as well as column hashers.
  define_features();
  init_weights();
  if (dt_fi == nullptr) create_fi();
  auto hashers = create_hashers(dt_X_train);

  // Obtain rowindex and data pointers for the target column(s).
  const Column& target_col0_train = dt_y_train->get_column(0);
  auto data_fi = static_cast<T*>(dt_fi->get_column(1).get_data_editable());

  // Training settings. By default each training iteration consists of
  // `dt_X_train->nrows` rows.
  size_t niterations = params.nepochs;
  size_t iteration_nrows = dt_X_train->nrows();
  size_t total_nrows = niterations * iteration_nrows;
  size_t iteration_end;

  // If a validation set is provided, we adjust batch size to `nepochs_val`.
  // After each batch, we calculate loss on the validation dataset,
  // and do early stopping if relative loss does not decrese by at least
  // `val_error`.
  bool validation = !std::isnan(nepochs_val);
  T loss = T_NAN; // This value is returned when validation is not enabled
  T loss_old = T(0); // Value of `loss` for a previous iteraction
  std::vector<T> loss_history;
  std::vector<hasherptr> hashers_val;
  const Column& target_col0_val = validation? dt_y_val->get_column(0)
                                            : target_col0_train;  // whatever
  if (validation) {
    hashers_val = create_hashers(dt_X_val);
    iteration_nrows = static_cast<size_t>(nepochs_val * dt_X_train->nrows());
    niterations = total_nrows / iteration_nrows;
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
  job.set_message("Fitting");
  NThreads nthreads = nthreads_from_niters(iteration_nrows, MIN_ROWS_PER_THREAD);

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
        dt::nested_for_static(iteration_size, ChunkSize(MIN_ROWS_PER_THREAD), [&](size_t i) {
          size_t ii = (iteration_start + i) % dt_X_train->nrows();
          U value;
          bool isvalid = target_col0_train.get_element(ii, &value);

          if (isvalid && _isfinite(value)) {
            hash_row(x, hashers, ii);
            for (size_t k = 0; k < label_ids_train.size(); ++k) {
              T p = linkfn(predict_row(
                      x, w, k,
                      [&](size_t f_id, T f_imp) {
                        fi[f_id] += f_imp;
                      }
                    ));

              // `targetfn` returns the actual target for the k-th model,
              // depending on the data in the y column and the label indicator.
              // When we do multilabel, we will have a set of y columns
              // and some sort of loop over their values here.
              U y = targetfn(value, label_ids_train[k]);
              update(x, w, p, y, k);
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
            V value;
            bool isvalid = target_col0_val.get_element(i, &value);

            if (isvalid && _isfinite(value)) {
              hash_row(x, hashers_val, i);
              for (size_t k = 0; k < label_ids_val.size(); ++k) {
                T p = linkfn(predict_row(
                        x, w, k, [&](size_t, T){}
                      ));
                V y = targetfn_val(value, label_ids_val[k]);
                loss_local += lossfn(p, y);
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
            T loss_current = loss_global.load() / (dt_X_val->nrows() * dt_y_val->ncols());
            loss_history[iter % val_niters] = loss_current / val_niters;
            loss = std::accumulate(loss_history.begin(), loss_history.end(), T(0));
            T loss_diff = (loss_old - loss) / loss_old;
            bool is_loss_bad = (iter >= val_niters) && (loss < T_EPSILON || loss_diff < val_error);
            loss_old = is_loss_bad? T_NAN : loss;
          }
          barrier();

          if (std::isnan(loss_old)) {
            if (dt::this_thread_index() == 0) {
              job.set_message("Fitting: early stopping criteria is met");
              // In some cases this makes progress "jumping" to 100%.
              job.set_done_amount(work_total);
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
  job.done();

  // Reset model stats after training, so that min gets re-computed
  // in `py::Validator::has_negatives()` during unpickling.
  reset_model_stats();

  double epoch_stopped = static_cast<double>(iteration_end) / dt_X_train->nrows();
  FtrlFitOutput res = {epoch_stopped, static_cast<double>(loss)};

  return res;
}


/**
 *  Make a prediction for an array of hashed features.
 */
template <typename T>
template <typename F>
T Ftrl<T>::predict_row(const uint64ptr& x, tptr<T>& w, size_t k, F fifn) {
  T wTx = T(0);
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T absw = std::max(std::abs(z[k][j]) - lambda1, T(0)) /
             (std::sqrt(n[k][j]) * ialpha + gamma);
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
void Ftrl<T>::update(const uint64ptr& x,
                     const tptr<T>& w,
                     T p,
                     U y,
                     size_t k)
{
  T g = p - static_cast<T>(y);
  T gsq = g * g;
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T sigma = (std::sqrt(n[k][j] + gsq) - std::sqrt(n[k][j])) * ialpha;
    z[k][j] += g - sigma * w[i];
    n[k][j] += gsq;
  }
}


/**
 *  Predict on a datatable and return a new datatable with
 *  the predicted probabilities.
 */
template <typename T>
dtptr Ftrl<T>::predict(const DataTable* dt_X) {
  if (!is_model_trained()) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }

  // Initialize helper parameters for prediction formula.
  init_helper_params();

  // Re-acquire model weight pointers.
  init_weights();

  // Re-create hashers, as stypes for predictions may be different.
  auto hashers = create_hashers(dt_X);

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
    case FtrlModelType::REGRESSION  : linkfn = identity<T>; break;
    case FtrlModelType::BINOMIAL    : linkfn = sigmoid<T>; break;
    case FtrlModelType::MULTINOMIAL : (nlabels < 3)? linkfn = sigmoid<T> :
                                                     linkfn = std::exp;
                                      break;
    default : throw ValueError() << "Cannot do any predictions, "
                                 << "the model was trained in an unknown mode";
  }

  NThreads nthreads = nthreads_from_niters(dt_X->nrows(), MIN_ROWS_PER_THREAD);
  bool k_binomial;

  // Set progress reporting
  size_t work_total = dt_X->nrows() / nthreads.get();
  dt::progress::work job(work_total);
  job.set_message("Predicting");

  dt::parallel_region(NThreads(nthreads), [&]() {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    tptr<T> w = tptr<T>(new T[nfeatures]);

    dt::nested_for_static(dt_X->nrows(), [&](size_t i) {
      // Predicting for all the `nlabels`
      hash_row(x, hashers, i);
      for (size_t k = 0; k < nlabels; ++k) {
        size_t label_id = static_cast<size_t>(data_label_ids[k]);
        if (model_type == FtrlModelType::BINOMIAL && label_id == 1) {
          k_binomial = k;
          continue;
        }

        data_p[k][i] = linkfn(predict_row(x, w, label_id, [&](size_t, T){}));
      }

      // Progress reporting
      if (dt::this_thread_index() == 0) {
        job.add_done_amount(1);
      }
    });
  });
  job.done();

  if (model_type == FtrlModelType::BINOMIAL) {
    dt::parallel_for_static(dt_X->nrows(), [&](size_t i){
      data_p[k_binomial][i] = T(1) - data_p[!k_binomial][i];
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
 *  Normalize rows in a datatable, so that their values sum up to 1.
 */
template <typename T>
void Ftrl<T>::normalize_rows(dtptr& dt) {
  size_t nrows = dt->nrows();
  size_t ncols = dt->ncols();

  std::vector<T*> data(ncols);
  for (size_t j = 0; j < ncols; ++j) {
    data[j] = static_cast<T*>(dt->get_column(j).get_data_editable());
  }

  dt::parallel_for_static(nrows, [&](size_t i){
    T sum = T(0);
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
  size_t nlabels = (dt_labels == nullptr)? 0 : dt_labels->nrows();
  size_t ncols = (model_type == FtrlModelType::BINOMIAL)? 2 : 2 * nlabels;

  colvec cols;
  cols.reserve(ncols);
  constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
  for (size_t i = 0; i < ncols; ++i) {
    cols.push_back(Column::new_data_column(params.nbins, stype));
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
  size_t ncols_model = dt_model->ncols();
  size_t ncols_model_new = 2 * dt_labels->nrows();
  xassert(ncols_model_new > ncols_model)

  colvec cols;
  cols.reserve(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols.push_back(dt_model->get_column(i));
  }

  Column zcol, ncol;
  // If `negative_class` parameter is set to `True`, all the new classes
  // get a copy of `z` and `n` weights of the `_negative_class`.
  // Otherwise, new classes start learning from zero weights.
  if (params.negative_class) {
    zcol = dt_model->get_column(0);
    ncol = dt_model->get_column(1);
  } else {
    const SType stype = dt_model->get_column(0).stype();
    Column col = Column::new_data_column(params.nbins, stype);
    auto data = static_cast<T*>(col.get_data_editable());
    std::memset(data, 0, params.nbins * sizeof(T));
    zcol = col;
    ncol = col;
  }

  for (size_t i = ncols_model; i < ncols_model_new; i+=2) {
    cols.push_back(zcol);
    cols.push_back(ncol);
  }

  dt_model = dtptr(new DataTable(std::move(cols), DataTable::default_names));
}


/**
 *  Create datatable for predictions.
 */
template <typename T>
dtptr Ftrl<T>::create_p(size_t nrows) {
  size_t nlabels = dt_labels->nrows();
  xassert(nlabels > 0);

  Column col0_str64 = dt_labels->get_column(0).cast(SType::STR64);

  strvec labels_vec(nlabels);

  for (size_t i = 0; i < nlabels; ++i) {
    CString val;
    bool isvalid = col0_str64.get_element(i, &val);
    labels_vec[i] = isvalid? std::string(val.ch, static_cast<size_t>(val.size))
                           : std::string();
  }

  colvec cols;
  cols.reserve(nlabels);
  constexpr SType stype = sizeof(T) == 4? SType::FLOAT32 : SType::FLOAT64;
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
void Ftrl<T>::reset() {
  dt_model = nullptr;
  dt_fi = nullptr;
  model_type = FtrlModelType::NONE;
  dt_labels = nullptr;
  colname_hashes.clear();
  interactions.clear();
}


/**
 *  Initialize model coefficients with zeros.
 */
template <typename T>
void Ftrl<T>::init_model() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols(); ++i) {
    auto data = static_cast<T*>(dt_model->get_column(i).get_data_editable());
    std::memset(data, 0, params.nbins * sizeof(T));
  }
}


/**
 *  Reset model stats.
 */
 template <typename T>
 void Ftrl<T>::reset_model_stats() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols(); ++i) {
    (dt_model->get_column(i)).reset_stats();
  }
}


/**
 *  Obtain pointers to the model column data.
 */
template <typename T>
void Ftrl<T>::init_weights() {
  size_t model_ncols = dt_model->ncols();
  xassert(model_ncols % 2 == 0);
  size_t nlabels = model_ncols / 2;
  z.clear();
  z.reserve(nlabels);
  n.clear();
  n.reserve(nlabels);

  for (size_t k = 0; k < nlabels; ++k) {
    z.push_back(static_cast<T*>(dt_model->get_column(2 * k).get_data_editable()));
    n.push_back(static_cast<T*>(dt_model->get_column(2 * k + 1).get_data_editable()));
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
void Ftrl<T>::init_fi() {
  if (dt_fi == nullptr) return;
  auto data = static_cast<T*>(dt_fi->get_column(1).get_data_editable());
  std::memset(data, 0, nfeatures * sizeof(T));
}


/**
 *  Determine number of features.
 */
template <typename T>
void Ftrl<T>::define_features() {
  nfeatures = dt_X_train->ncols() + interactions.size();
}


/**
 *  Create hashers for all datatable column.
 */
template <typename T>
std::vector<hasherptr> Ftrl<T>::create_hashers(const DataTable* dt) {
  std::vector<hasherptr> hashers;
  hashers.clear();
  hashers.reserve(dt->ncols());

  // Create hashers.
  for (size_t i = 0; i < dt->ncols(); ++i) {
    const Column& col = dt->get_column(i);
    hashers.push_back(create_hasher(col));
  }

  // Hash column names.
  const std::vector<std::string>& c_names = dt->get_names();
  colname_hashes.clear();
  colname_hashes.reserve(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); i++) {
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
hasherptr Ftrl<T>::create_hasher(const Column& col) {
  int shift_nbits = DOUBLE_MANTISSA_NBITS - params.mantissa_nbits;
  switch (col.stype()) {
    case SType::BOOL:
    case SType::INT8:    return hasherptr(new HasherInt<int8_t>(col));
    case SType::INT16:   return hasherptr(new HasherInt<int16_t>(col));
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
    x[i] = (hashers[i]->hash(row) + colname_hashes[i]) % params.nbins;
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
      x[i] %= params.nbins;
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
py::oobj Ftrl<T>::get_model() {
  if (dt_model == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_model));
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
py::oobj Ftrl<T>::get_fi(bool normalize /* = true */) {
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


/**
 *  Other getters, setters and initializers.
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
py::oobj Ftrl<T>::get_labels() {
  if (dt_labels == nullptr) return py::None();
  return py::Frame::oframe(new DataTable(*dt_labels));
}


template <typename T>
void Ftrl<T>::set_model(const DataTable& dt_model_in) {
  dt_model = dtptr(new DataTable(dt_model_in));
  set_nbins(dt_model->nrows());
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
void Ftrl<T>::set_fi(const DataTable& dt_fi_in) {
  dt_fi = dtptr(new DataTable(dt_fi_in));
  nfeatures = dt_fi->nrows();
}



template <typename T>
void Ftrl<T>::init_helper_params() {
  ialpha = T(1) / alpha;
  gamma = beta * ialpha + lambda2;
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
}


template <typename T>
void Ftrl<T>::set_mantissa_nbits(unsigned char mantissa_nbits_in) {
  xassert(mantissa_nbits_in >= 0);
  xassert(mantissa_nbits_in <= DOUBLE_MANTISSA_NBITS);
  params.mantissa_nbits = mantissa_nbits_in;
}


template <typename T>
void Ftrl<T>::set_interactions(std::vector<intvec> interactions_in) {
  interactions = std::move(interactions_in);
}


template <typename T>
void Ftrl<T>::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
}


template <typename T>
void Ftrl<T>::set_negative_class(bool negative_class_in) {
  params.negative_class = negative_class_in;
}


template <typename T>
void Ftrl<T>::set_labels(const DataTable& dt_labels_in) {
  dt_labels = dtptr(new DataTable(dt_labels_in));
}


template class Ftrl<float>;
template class Ftrl<double>;


} // namespace dt
