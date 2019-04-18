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


namespace dt {


/**
 *  Constructor. Set up parameters and initialize weights.
 */
template <typename T>
Ftrl<T>::Ftrl(FtrlParams params_in) :
  model_type(FtrlModelType::NONE),
  params(params_in),
  alpha(static_cast<T>(params_in.alpha)),
  beta(static_cast<T>(params_in.beta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nbins(params_in.nbins),
  mantissa_nbits(params_in.mantissa_nbits),
  nepochs(params_in.nepochs),
  nfeatures(0),
  dt_X(nullptr),
  dt_y(nullptr),
  dt_X_val(nullptr),
  dt_y_val(nullptr),
  nepochs_val(T_NAN),
  val_error(T_NAN)
{
}


/**
 *  Depending on the target column stype, this method does
 *  - binomial logistic regression (BOOL);
 *  - multinomial logistic regression (STR32, STR64);
 *  - numerical regression (INT8, INT16, INT32, INT64, FLOAT32, FLOAT64).
 *  and returns epoch at which learning completed or was early stopped.
 */
template <typename T>
FtrlFitOutput Ftrl<T>::dispatch_fit(const DataTable* dt_X_in,
                                 const DataTable* dt_y_in,
                                 const DataTable* dt_X_val_in,
                                 const DataTable* dt_y_val_in,
                                 double nepochs_val_in,
                                 double val_error_in) {
  dt_X = dt_X_in;
  dt_y = dt_y_in;
  dt_X_val = dt_X_val_in;
  dt_y_val = dt_y_val_in;
  nepochs_val = static_cast<T>(nepochs_val_in);
  val_error = static_cast<T>(val_error_in);
  FtrlFitOutput res;

  SType stype_y = dt_y->columns[0]->stype();
  switch (stype_y) {
    case SType::BOOL:    res = fit_binomial(); break;
    case SType::INT8:    res = fit_regression<int8_t>(); break;
    case SType::INT16:   res = fit_regression<int16_t>(); break;
    case SType::INT32:   res = fit_regression<int32_t>(); break;
    case SType::INT64:   res = fit_regression<int64_t>(); break;
    case SType::FLOAT32: res = fit_regression<float>(); break;
    case SType::FLOAT64: res = fit_regression<double>(); break;
    case SType::STR32:   FALLTHROUGH;
    case SType::STR64:   res = fit_multinomial(); break;
    default:             throw TypeError() << "Targets of type `"
                                           << stype_y << "` are not supported";
  }

  dt_X = nullptr;
  dt_y = nullptr;
  dt_X_val = nullptr;
  dt_y_val = nullptr;
  nepochs_val = T_NAN;
  val_error = T_NAN;
  map_val.clear();

  return res;
}



template <typename T>
FtrlFitOutput Ftrl<T>::fit_binomial() {
  xassert(dt_y->ncols == 1);
  if (model_type != FtrlModelType::NONE &&
      model_type != FtrlModelType::BINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from binomial. To train it "
                         "in a binomial mode this model should be reset.";
  }
  if (model_type == FtrlModelType::NONE) {
    labels = dt_y->get_names();
    create_model();
    model_type = FtrlModelType::BINOMIAL;
  }
  map_val = {0};
  return fit<int8_t>(sigmoid<T>, log_loss<T>);
}


template <typename T>
template <typename U>
FtrlFitOutput Ftrl<T>::fit_regression() {
  xassert(dt_y->ncols == 1);
  if (model_type != FtrlModelType::NONE &&
      model_type != FtrlModelType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }
  if (model_type == FtrlModelType::NONE) {
    labels = dt_y->get_names();
    create_model();
    model_type = FtrlModelType::REGRESSION;
  }
  map_val = {0};
  return fit<U>(identity<T>, squared_loss<T, U>);
}


template <typename T>
FtrlFitOutput Ftrl<T>::fit_multinomial() {
  if (model_type != FtrlModelType::NONE &&
      model_type != FtrlModelType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a multinomial mode this model should be reset.";
  }

  if (model_type == FtrlModelType::NONE) {
    xassert(labels.size() == 0);
    xassert(dt_model == nullptr);
    if (params.negative_class) {
      labels.push_back("_negative");
    }
    create_model();
    model_type = FtrlModelType::MULTINOMIAL;
  }

  dtptr dt_y_train = create_y_train();
  dt_y = dt_y_train.get();

  // Create validation targets if needed.
  dtptr dt_y_val_filtered;
  if (!std::isnan(nepochs_val)) {
    dt_y_val_filtered = create_y_val();
    dt_y_val = dt_y_val_filtered.get();
  }

  return fit<int8_t>(sigmoid<T>, log_loss<T>);
}


/**
 *  Create training targets.
 */
template <typename T>
dtptr Ftrl<T>::create_y_train() {
  // Do nhot encoding and sort labels alphabetically.
  dtptr dt_y_nhot = dtptr(split_into_nhot(
                      dt_y->columns[0],
                      dt::FtrlBase::SEPARATOR,
                      true // also do sorting
                    ));
  strvec labels_in = dt_y_nhot->get_names();

  colvec cols;
  cols.reserve(labels.size());

  // Create a target column with all negatives.
  colptr col_negative = std::unique_ptr<Column>(
                          create_negative_column(dt_y_nhot->nrows)
                        );

  if (params.negative_class) {
    cols.push_back(col_negative->shallowcopy());
  }

  // First, process labels that are already in the model.
  for (size_t i = params.negative_class; i < labels.size(); ++i) {
    auto it = find(labels_in.begin(), labels_in.end(), labels[i]);
    if (it == labels_in.end()) {
      // If existing label is not found in the new label list,
      // train it on all the negatives.
      cols.push_back(col_negative->shallowcopy());
    } else {
      // Otherwise, use the actual targets.
      auto pos = static_cast<size_t>(std::distance(labels_in.begin(), it));
      cols.push_back(dt_y_nhot->columns[pos]->shallowcopy());
      labels_in[pos] = "";
    }
  }

  // Second, process new labels.
  size_t n_new_labels = 0;
  for (size_t i = 0; i < labels_in.size(); ++i) {
    if (labels_in[i] == "") continue;
    cols.push_back(dt_y_nhot->columns[i]->shallowcopy());
    labels.push_back(labels_in[i]);
    n_new_labels++;
  }

  // Add new model columns for the new labels. The new columns are
  // shallow copies of the corresponding ones for the "_negative" classifier.
  if (n_new_labels) adjust_model();

  return dtptr(new DataTable(std::move(cols)));
}


/**
 *  Create a target frame with all the negatives.
 */
template <typename T>
Column* Ftrl<T>::create_negative_column(size_t nrows) {
  Column* col = Column::new_data_column(SType::BOOL, nrows);
  auto data = static_cast<bool*>(col->data_w());
  std::memset(data, 0, nrows * sizeof(bool));
  return col;
}


/**
 *  Create validation targets for early stopping. Only include the labels
 *  the model was already trained on. Also, create mapping between the validation
 *  labels and the model labels to be used during training.
 */
template <typename T>
dtptr Ftrl<T>::create_y_val() {
  xassert(map_val.size() == 0);
  xassert(dt_X_val != nullptr && dt_y_val != nullptr)
  xassert(dt_X_val->nrows == dt_y_val->nrows)

  dtptr dt_y_val_nhot = dtptr(split_into_nhot(dt_y_val->columns[0], dt::FtrlBase::SEPARATOR));
  const strvec& labels_val = dt_y_val_nhot->get_names();
  xassert(dt_y_val_nhot->nrows == dt_y_val->nrows)
  colvec cols;

  // First, add a "_negative" target column and its mapping info.
  if (params.negative_class) {
    Column* col = create_negative_column(dt_y_val_nhot->nrows);
    cols.push_back(col);
    map_val.push_back(0);
  }

  // Second, filter out only the model known labels.
  for (size_t i = 0; i < labels_val.size(); ++i) {
    auto it = find(labels.begin(), labels.end(), labels_val[i]);
    if (it == labels.end()) continue;

    cols.push_back(dt_y_val_nhot->columns[i]->shallowcopy());
    auto pos = static_cast<size_t>(std::distance(labels.begin(), it));
    map_val.push_back(pos);
  }

  return dtptr(new DataTable(std::move(cols)));
}


/**
 *  Fit model on a datatable.
 */
template <typename T>
template <typename U> /* target column(s) data type */
FtrlFitOutput Ftrl<T>::fit(T(*linkfn)(T), T(*lossfn)(T,U)) {
  // Define features, weight pointers, feature importances storage,
  // as well as column hashers.
  define_features();
  init_weights();
  if (dt_fi == nullptr) create_fi();
  auto hashers = create_hashers(dt_X);

  // Obtain rowindex and data pointers for the target column(s).
  std::vector<RowIndex> ri, ri_val;
  std::vector<const U*> data, data_val;
  fill_ri_data<U>(dt_y, ri, data);
  auto data_fi = static_cast<T*>(dt_fi->columns[1]->data_w());

  // Training settings. By default each training iteration consists of
  // `dt_X->nrows` rows.
  size_t niterations = nepochs;
  size_t iteration_nrows = dt_X->nrows;
  size_t total_nrows = niterations * iteration_nrows;

  // If a validation set is provided, we adjust batch size to `nepochs_val`.
  // After each batch, we calculate loss on the validation dataset,
  // and do early stopping if relative loss does not decrese by at least
  // `val_error`.
  bool validation = !std::isnan(nepochs_val);
  T loss = T_NAN; // This value is returned when validation is not enabled
  T loss_old = 0; // Value of `loss` for a previous iteraction
  std::vector<hasherptr> hashers_val;
  if (validation) {
    hashers_val = create_hashers(dt_X_val);
    iteration_nrows = static_cast<size_t>(nepochs_val * dt_X->nrows);
    niterations = total_nrows / iteration_nrows;
    fill_ri_data<U>(dt_y_val, ri_val, data_val);
  }


  std::mutex m;
  size_t iteration_end = 0;

  // If we request more threads than is available, `dt::parallel_region()`
  // will fall back to the possible maximum.
  size_t nthreads = std::max(iteration_nrows / dt::FtrlBase::MIN_ROWS_PER_THREAD, 1lu);
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
        dt::parallel_for_static(iteration_size, [&](size_t i) {
          size_t ii = (iteration_start + i) % dt_X->nrows;
          const size_t j0 = ri[0][ii];
          // Note that for FtrlModelType::BINOMIAL and FtrlModelType::REGRESSION
          // dt_y has only one column that may contain NA's or be a view
          // with an NA rowindex. For FtrlModelType::MULTINOMIAL we have as many
          // columns as there are labels, and split_into_nhot() filters out
          // NA's and can never be a view. Therefore, to ignore NA targets
          // it is enough to check the condition below for the zero column only.
          // FIXME: this condition can be removed for FtrlModelType::MULTINOMIAL.
          if (j0 != RowIndex::NA && !ISNA<U>(data[0][j0])) {
            hash_row(x, hashers, ii);
            for (size_t k = 0; k < dt_y->ncols; ++k) {
              const size_t j = ri[k][ii];
              T p = linkfn(predict_row(
                      x, w, k,
                      [&](size_t f_id, T f_imp) {
                        fi[f_id] += f_imp;
                      }
                    ));
              update(x, w, p, data[k][j], k);
            }
          }
        }); // End training.
        barrier();

        // Validation and early stopping.
        if (validation) {
          dt::atomic<T> loss_global { 0.0 };
          T loss_local = 0.0;

          dt::parallel_for_static(dt_X_val->nrows, [&](size_t i) {
            const size_t j0 = ri_val[0][i];
            // This condition is kind of the same as for training, see comment
            // above.
            if (j0 != RowIndex::NA && !ISNA<U>(data_val[0][j0])) {
              hash_row(x, hashers_val, i);
              for (size_t k = 0; k < dt_y_val->ncols; ++k) {
                const size_t j = ri_val[k][i];
                T p = linkfn(predict_row(
                        x, w, map_val[k], [&](size_t, T){}
                      ));
                loss_local += lossfn(p, data_val[k][j]);
              }
            }
          });
          loss_global.fetch_add(loss_local);
          barrier();

          // Thread #0 checks relative loss change and, if it does not decrease
          // more than `val_error`, sets `loss_old` to `NaN` -> this will stop
          // all the threads after `barrier()`.
          if (dt::this_thread_index() == 0) {
            loss = loss_global.load() / (dt_X_val->nrows * dt_y_val->ncols);
            T loss_diff = (loss_old - loss) / loss_old;
            bool is_loss_bad = iter && (loss < T_EPSILON || loss_diff < val_error);
            loss_old = is_loss_bad? T_NAN : loss;
          }
          barrier();

          if (std::isnan(loss_old)) break;
        } // End validation

        // Update global feature importances with the local data.
        std::lock_guard<std::mutex> lock(m);
        for (size_t i = 0; i < nfeatures; ++i) {
          data_fi[i] += fi[i];
        }

      } // End iteration.

    }
  );

  double epoch_stopped = static_cast<double>(iteration_end) / dt_X->nrows;
  FtrlFitOutput res = {epoch_stopped, static_cast<double>(loss)};
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
 *  Predict on a datatable and return a new datatable with
 *  the predicted probabilities.
 */
template <typename T>
dtptr Ftrl<T>::predict(const DataTable* dt_X_in) {
  if (model_type == FtrlModelType::NONE) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }
  dt_X = dt_X_in;
  init_weights();

  // Re-create hashers, as stypes for predictions may be different.
  auto hashers = create_hashers(dt_X);

  // Create datatable for predictions and obtain column data pointers.
  size_t nlabels = labels.size();
  dtptr dt_p = create_p(dt_X->nrows);
  std::vector<T*> data_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    data_p[i] = static_cast<T*>(dt_p->columns[i]->data_w());
  }

  // Determine which link function we should use.
  T (*linkfn)(T);
  switch (model_type) {
    case FtrlModelType::REGRESSION  : linkfn = identity<T>; break;
    case FtrlModelType::BINOMIAL    : linkfn = sigmoid<T>; break;
    case FtrlModelType::MULTINOMIAL : (nlabels < 3)? linkfn = sigmoid<T> :
                                                     linkfn = std::exp;
                                      break;
    default : throw ValueError() << "Cannot make any predictions, "
                                 << "the model was trained in an unknown mode";
  }


  size_t nthreads = dt_X->nrows / dt::FtrlBase::MIN_ROWS_PER_THREAD;
  nthreads = std::min(std::max(nthreads, 1lu), dt::num_threads_in_pool());

  dt::parallel_region(nthreads, [&]() {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    tptr<T> w = tptr<T>(new T[nfeatures]);

    dt::parallel_for_static(dt_X->nrows, [&](size_t i){
      hash_row(x, hashers, i);
      for (size_t k = 0; k < nlabels; ++k) {
        data_p[k][i] = linkfn(predict_row(x, w, k, [&](size_t, T){}));
      }
    });
  });

  // For multinomial case, when there is two labels, we match binomial
  // classifier by using `sigmoid` link function. When there is more
  // than two labels, we use `std::exp` link function, and do normalization,
  // so that predictions sum up to 1, effectively doing `softmax` linking.
  if (nlabels > 2) normalize_rows(dt_p);
  dt_X = nullptr;
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
    data.push_back(static_cast<const U*>(dt->columns[i]->data()));
    ri.push_back(dt->columns[i]->rowindex());
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
    data[j] = static_cast<T*>(dt->columns[j]->data_w());
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
  size_t nlabels = labels.size();

  size_t ncols = 2 * nlabels;
  colvec cols(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    cols[i] = new RealColumn<T>(nbins);
  }
  dt_model = dtptr(new DataTable(std::move(cols)));
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
  size_t ncols_model_new = 2 * labels.size();
  xassert(ncols_model_new > ncols_model)

  colvec cols(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols[i] = dt_model->columns[i]->shallowcopy();
  }

  colvec cols_new(2);
  colptr col;
  // If we have a negative class, then all the new classes
  // get a copy of its weights to start learning from.
  // Otherwise, new classes start learning from zero weights.
  if (params.negative_class) {
    cols_new[0] = dt_model->columns[0];
    cols_new[1] = dt_model->columns[1];
  } else {
    col = std::unique_ptr<Column>(new RealColumn<T>(nbins));
    auto data = static_cast<T*>(col->data_w());
    std::memset(data, 0, nbins * sizeof(T));
    cols_new[0] = col.get();
    cols_new[1] = col.get();
  }

  for (size_t i = ncols_model; i < ncols_model_new; i+=2) {
    cols[i] = cols_new[0]->shallowcopy();
    cols[i + 1] = cols_new[1]->shallowcopy();
  }

  dt_model = dtptr(new DataTable(std::move(cols)));
}


/**
 *  Create datatable for predictions.
 */
template <typename T>
dtptr Ftrl<T>::create_p(size_t nrows) {
  size_t nlabels = labels.size();
  xassert(nlabels > 0);

  colvec cols(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    cols[i] = new RealColumn<T>(nrows);
  }
  dtptr dt_p = dtptr(new DataTable(std::move(cols), labels));
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
  labels.clear();
  colname_hashes.clear();
  interactions.clear();
}


/**
 *  Initialize model coefficients with zeros.
 */
template <typename T>
void Ftrl<T>::init_model() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols; ++i) {
    auto data = static_cast<T*>(dt_model->columns[i]->data_w());
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
    z.push_back(static_cast<T*>(dt_model->columns[2 * k]->data_w()));
    n.push_back(static_cast<T*>(dt_model->columns[2 * k + 1]->data_w()));
  }
}


/**
 * Create feature importance datatable.
 */
template <typename T>
void Ftrl<T>::create_fi() {
  const strvec& colnames = dt_X->get_names();

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

  Column* c_fi_values = new RealColumn<T>(nfeatures);
  dt_fi = dtptr(new DataTable({std::move(c_fi_names).to_column(), c_fi_values},
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
  auto data = static_cast<T*>(dt_fi->columns[1]->data_w());
  std::memset(data, 0, nfeatures * sizeof(T));
}


/**
 *  Determine number of features.
 */
template <typename T>
void Ftrl<T>::define_features() {
  nfeatures = dt_X->ncols + interactions.size();
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
    Column* col = dt->columns[i];
    hashers.push_back(create_hasher(col));
  }

  // Hash column names.
  const std::vector<std::string>& c_names = dt->get_names();
  colname_hashes.clear();
  colname_hashes.reserve(dt->ncols);
  for (size_t i = 0; i < dt->ncols; i++) {
    uint64_t h = hash_murmur2(c_names[i].c_str(),
                             c_names[i].length() * sizeof(char),
                             0);
    colname_hashes.push_back(h);
  }

  return hashers;
}


/**
 *  Depending on a column type, create a corresponding hasher.
 */
template <typename T>
hasherptr Ftrl<T>::create_hasher(const Column* col) {
  unsigned char shift_nbits = dt::FtrlBase::DOUBLE_MANTISSA_NBITS - mantissa_nbits;
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    return hasherptr(new HasherBool(col));
    case SType::INT8:    return hasherptr(new HasherInt<int8_t>(col));
    case SType::INT16:   return hasherptr(new HasherInt<int16_t>(col));
    case SType::INT32:   return hasherptr(new HasherInt<int32_t>(col));
    case SType::INT64:   return hasherptr(new HasherInt<int64_t>(col));
    case SType::FLOAT32: return hasherptr(new HasherFloat<float>(col, shift_nbits));
    case SType::FLOAT64: return hasherptr(new HasherFloat<double>(col, shift_nbits));
    case SType::STR32:   return hasherptr(new HasherString<uint32_t>(col));
    case SType::STR64:   return hasherptr(new HasherString<uint64_t>(col));
    default:             throw  TypeError() << "Cannot hash a column of type "
                                            << stype;
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
  for (size_t i = 0; i < dt_X->ncols; ++i) {
    x[i] = (hashers[i]->hash(row) + colname_hashes[i]) % nbins;
  }

  // Do feature interactions.
  if (interactions.size() > 0) {
    size_t count = 0;
    for (auto interaction : interactions) {
      size_t i = dt_X->ncols + count;
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
bool Ftrl<T>::is_trained() {
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
    auto col = static_cast<RealColumn<T>*>(dt_fi_copy->columns[1]);
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
const std::vector<sizetvec>& Ftrl<T>::get_interactions() {
  return interactions;
}


template <typename T>
size_t Ftrl<T>::get_nepochs() {
  return params.nepochs;
}


template <typename T>
bool Ftrl<T>::get_double_precision() {
  return params.double_precision;
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
const strvec& Ftrl<T>::get_labels() {
  return labels;
}


template <typename T>
void Ftrl<T>::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_nbins(dt_model->nrows);
  nfeatures = 0;
}


template <typename T>
void Ftrl<T>::set_model_type(FtrlModelType model_type_in) {
  model_type = model_type_in;
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
void Ftrl<T>::set_interactions(std::vector<sizetvec> interactions_in) {
  interactions = std::move(interactions_in);
}


template <typename T>
void Ftrl<T>::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
  nepochs = nepochs_in;
}


template <typename T>
void Ftrl<T>::set_double_precision(bool double_precision_in) {
  params.double_precision = double_precision_in;
}


template <typename T>
void Ftrl<T>::set_negative_class(bool negative_class_in) {
  params.negative_class = negative_class_in;
}


template <typename T>
void Ftrl<T>::set_labels(strvec labels_in) {
  labels = labels_in;
}


template class Ftrl<float>;
template class Ftrl<double>;


} // namespace dt
