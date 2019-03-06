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
#ifndef dt_MODELS_FTRL_REAL_h
#define dt_MODELS_FTRL_REAL_h
#include "py_datatable.h"
#include "str/py_str.h"
#include "utils/parallel.h"
#include "models/hasher.h"
#include "models/utils.h"


namespace dt {

template <typename T /* float or double */>
class FtrlReal : public dt::Ftrl {
  private:
    // Model datatable with the shape of (nbins, 2 * nlabels),
    // and pointers to coefficients.
    dtptr dt_model;
    std::vector<T*> z, n;

    // Feature importances datatable with the shape of (nfeatures, 2),
    // where first column contains feature names and the second one
    // feature importance values.
    dtptr dt_fi;

    // Fitting parameters
    FtrlParams params;
    T alpha;
    T beta;
    T lambda1;
    T lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;
    bool double_precision;
    size_t: 40;

    // Whether or not the model was trained, `false` by default.
    // `fit(...)` and `set_model(...)` methods will set this to `true`.
    bool model_trained;
    FtrlModelType model_type;

    // Vector of labels detected in a target frame.
    strvec labels;

    // Number of columns in a fitting datatable and a total number of features
    size_t dt_X_ncols;
    size_t nfeatures;


    // Vector of hashed column names
    std::vector<uint64_t> colnames_hashes;

  public:
    FtrlReal(FtrlParams);
    static const std::vector<std::string> model_colnames;

    // Fit dispatcher and fitting methods
    double dispatch_fit(const DataTable*, const DataTable*, const DataTable*, const DataTable*, double) override;
    template <typename U, typename F, typename G>
    double fit(const DataTable*, const DataTable*, const DataTable*, const DataTable*, double, F, G);
    template <typename U>
    double fit_regression(const DataTable*, const DataTable*, const DataTable*, const DataTable*, double);
    double fit_binomial(const DataTable*, const DataTable*, const DataTable*, const DataTable*, double);
    double fit_multinomial(const DataTable*, const DataTable*, const DataTable*, const DataTable*, double);


    // Predicting method
    dtptr predict(const DataTable*) override;
    T predict_row(const uint64ptr&, tptr<T>&, size_t);

    template <typename U>
    void update(const uint64ptr&, const tptr<T>&, T, U, size_t);
    dtptr create_dt_p(size_t);
    template <typename U>
    void fill_ri_data(const DataTable*, std::vector<const RowIndex>&, std::vector<const U*>&);

    // Model and feature importance handling methods
    void create_model();
    void adjust_model();
    void reset() override;
    void init_model();
    void init_weights();
    void create_fi(const DataTable*);
    void init_fi();
    void define_features(size_t);
    void normalize_rows(dtptr&);

    // Hashing methods
    std::vector<hasherptr> create_hashers(const DataTable*);
    static hasherptr create_hasher(const Column*);
    void hash_row(uint64ptr&, std::vector<hasherptr>&, size_t);

    // Model validation methods
    static bool is_dt_valid(const dtptr&, size_t, size_t);
    bool is_trained() override;

    // Getters
    DataTable* get_model() override;
    DataTable* get_fi(bool normalize = true) override;
    FtrlModelType get_model_type() override;
    size_t get_nfeatures() override;
    size_t get_dt_X_ncols() override;
    std::vector<uint64_t> get_colnames_hashes() override;
    double get_alpha() override;
    double get_beta() override;
    double get_lambda1() override;
    double get_lambda2() override;
    uint64_t get_nbins() override;
    size_t get_nepochs() override;
    bool get_interactions() override;
    bool get_double_precision() override;
    FtrlParams get_params() override;
    strvec get_labels() override;

    // Setters
    void set_model(DataTable*) override;
    void set_fi(DataTable*) override;
    void set_model_type(FtrlModelType) override;
    void set_alpha(double) override;
    void set_beta(double) override;
    void set_lambda1(double) override;
    void set_lambda2(double) override;
    void set_nbins(uint64_t) override;
    void set_nepochs(size_t) override;
    void set_interactions(bool) override;
    void set_double_precision(bool) override;
    void set_labels(strvec) override;
};



/*
*  Set up parameters and initialize weights.
*/
template <typename T>
FtrlReal<T>::FtrlReal(FtrlParams params_in) :
  params(params_in),
  alpha(static_cast<T>(params_in.alpha)),
  beta(static_cast<T>(params_in.beta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nbins(params_in.nbins),
  nepochs(params_in.nepochs),
  interactions(params_in.interactions),
  model_trained(false),
  model_type(FtrlModelType::NONE),
  dt_X_ncols(0),
  nfeatures(0)
{
}


/*
* Depending on the target column stype, this method does
* - binomial logistic regression (BOOL);
* - multinomial logistic regression (STR32, STR64);
* - numerical regression (INT8, INT16, INT32, INT64, FLOAT32, FLOAT64).
*/
template <typename T>
double FtrlReal<T>::dispatch_fit(const DataTable* dt_X, const DataTable* dt_y,
                               const DataTable* dt_X_val, const DataTable* dt_y_val,
                               double nepochs_val) {
  double epoch;
  SType stype_y = dt_y->columns[0]->stype();
  switch (stype_y) {
    case SType::BOOL:    epoch = fit_binomial(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::INT8:    epoch = fit_regression<int8_t>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::INT16:   epoch = fit_regression<int16_t>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::INT32:   epoch = fit_regression<int32_t>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::INT64:   epoch = fit_regression<int64_t>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::FLOAT32: epoch = fit_regression<float>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::FLOAT64: epoch = fit_regression<double>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    case SType::STR32:   [[clang::fallthrough]];
    case SType::STR64:   epoch = fit_multinomial(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val); break;
    default:             throw TypeError() << "Cannot predict for a target "
                                           << "of type `" << stype_y << "`";
  }
  model_trained = true;
  return epoch;
}



template <typename T>
double FtrlReal<T>::fit_binomial(const DataTable* dt_X, const DataTable* dt_y,
                               const DataTable* dt_X_val, const DataTable* dt_y_val,
                               double nepochs_val) {
  if (model_type != FtrlModelType::NONE && model_type != FtrlModelType::BINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from binomial. To train it "
                         "in a binomial mode this model should be reset.";
  }
  model_type = FtrlModelType::BINOMIAL;
  if (!labels.size()) {
    labels = dt_y->get_names();
    xassert(labels.size() == 1);
    create_model();
  }
  return fit<int8_t>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val, sigmoid<T>, log_loss<T>);
}


template <typename T>
template <typename U>
double FtrlReal<T>::fit_regression(const DataTable* dt_X, const DataTable* dt_y,
                               const DataTable* dt_X_val, const DataTable* dt_y_val,
                               double nepochs_val) {
  if (model_type != FtrlModelType::NONE && model_type != FtrlModelType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }
  model_type = FtrlModelType::REGRESSION;
  if (!labels.size()) {
    labels = dt_y->get_names();
    xassert(labels.size() == 1);
    create_model();
  }
  return fit<U>(dt_X, dt_y, dt_X_val, dt_y_val, nepochs_val, identity<T>, squared_loss<T, U>);
}


template <typename T>
double FtrlReal<T>::fit_multinomial(const DataTable* dt_X, const DataTable* dt_y,
                               const DataTable* dt_X_val, const DataTable* dt_y_val,
                               double nepochs_val) {
  if (model_type != FtrlModelType::NONE && model_type != FtrlModelType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a multinomial mode this model should be reset.";
  }

  if (model_type == FtrlModelType::NONE) {
    xassert(labels.size() == 0);
    xassert(dt_model == nullptr);
    xassert(model_trained == false);

    labels.push_back("FTRL_negative");
    create_model();
    model_type = FtrlModelType::MULTINOMIAL;
  }


  // Do one hot encoding and get the list of incoming labels
  dtptr dt_y_nhot = dtptr(split_into_nhot(dt_y->columns[0], '\0'));
  strvec labels_in = dt_y_nhot->get_names();

  // Create a "negative" column
  colvec cols;
  cols.reserve(labels.size());

  cols.push_back(Column::new_data_column(SType::BOOL, dt_y_nhot->nrows));
  auto data_negative = static_cast<bool*>(cols[0]->data_w());
  std::memset(data_negative, 0, dt_y->nrows * sizeof(bool));

  for (size_t i = 1; i < labels.size(); ++i) {
    auto it = find(labels_in.begin(), labels_in.end(), labels[i]);
    if (it == labels_in.end()) {
      // If existing label is not found in the new label list,
      // train it on all the negatives.
      cols.push_back(cols[0]->shallowcopy());
    } else {
      // Otherwise, use the actual targets.
      auto pos = static_cast<size_t>(std::distance(labels_in.begin(), it));
      cols.push_back(dt_y_nhot->columns[pos]->shallowcopy());
      labels_in[pos] = "";
    }
  }

  // The remaining labels are the new ones, for which we
  // need to add new model columns
  size_t n_new_labels = 0;
  for (size_t i = 0; i < labels_in.size(); ++i) {
    if (labels_in[i] == "") continue;

    cols.push_back(dt_y_nhot->columns[i]->shallowcopy());
    labels.push_back(labels_in[i]);
    n_new_labels++;
  }
  dtptr dt_y_train = dtptr(new DataTable(std::move(cols)));
  if (n_new_labels) adjust_model();


  dtptr dt_y_val_nhot;
  if (dt_y_val != nullptr) {
    dt_y_val_nhot = dtptr(split_into_nhot(dt_y_val->columns[0], '\0'));
  }

  // FIXME: add a negative column and check that dt_y_val_nhot doesn't have any labels not beeing present in dt_y_nhot
  return fit<int8_t>(dt_X, dt_y_train.get(), dt_X_val, dt_y_val_nhot.get(), nepochs_val, sigmoid<T>, log_loss<T>);
}


template <typename T>
template <typename U /* column data type */>
void FtrlReal<T>::fill_ri_data(const DataTable* dt,
                               std::vector<const RowIndex>& ri,
                               std::vector<const U*>& data) {
	size_t nlabels = labels.size();
  ri.reserve(nlabels);
  data.reserve(nlabels);
	for (size_t i = 0; i < nlabels; ++i) {
  	data.push_back(static_cast<const U*>(dt->columns[i]->data()));
	  ri.push_back(dt->columns[i]->rowindex());
	}
}

/*
*  Fit model on a datatable.
*/
template <typename T>
template <typename U /* column data type */, typename F /* link function */, typename G /* loss function */>
double FtrlReal<T>::fit(const DataTable* dt_X, const DataTable* dt_y,
                      const DataTable* dt_X_val, const DataTable* dt_y_val,
                      double nepochs_val, F link, G loss) {
	// Define features and labels
  define_features(dt_X->ncols);
  init_weights();

  // Initialize weights, create model or fi datatables
  if (!is_dt_valid(dt_fi, nfeatures, 2)) create_fi(dt_X);

  // Create column hashers
  auto hashers = create_hashers(dt_X);

  // Settings for parallel processing. By default we invoke `dt::run_interleaved()`
  // on all the data for all the epochs at once.
	size_t total_nrows = dt_X->nrows * nepochs;
  size_t nchunks = 1;
  size_t chunk_nrows = total_nrows;

  // If a validation set has been provided, we train on chunks of data.
  // After each chunk was fitted, we calculate loss on the validation set,
  // and do early stopping if needed.
  bool validation = !isnan(nepochs_val);
  T loss_global = 0;
  T loss_global_prev = 0;
  std::vector<hasherptr> hashers_val;
  if (validation) {
    hashers_val = create_hashers(dt_X_val); // FIXME: make sure column names are the same in dt_X
    chunk_nrows = static_cast<size_t>(nepochs_val * dt_X->nrows);
    nchunks = static_cast<size_t>(ceil(static_cast<double>(total_nrows) / chunk_nrows));
  }

  // Gather rowindex and data pointers
  std::vector<const RowIndex> ri, ri_val;
  std::vector<const U*> data, data_val;
  fill_ri_data<U>(dt_y, ri, data);
  if (validation) fill_ri_data<U>(dt_y_val, ri_val, data_val);

	size_t chunk_end = 0;
	for (size_t c = 0; c < nchunks; ++c) {
    size_t chunk_start = c * chunk_nrows;
    chunk_end = std::min((c + 1) * chunk_nrows, total_nrows);

	  dt::run_interleaved(
		  [&](size_t i0, size_t i1, size_t di) {
		    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
		    tptr<T> w = tptr<T>(new T[nfeatures]());
		    for (size_t i = chunk_start + i0; i < chunk_start + i1; i += di) {
		    	size_t ii = i % dt_X->nrows;
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
              T p = link(predict_row(x, w, k));
              update(x, w, p, data[k][j], k);
            }
          }
		    }

		  },
		  chunk_end - chunk_start
		);


    if (validation) {
		  dt::run_interleaved(
			  [&](size_t i0, size_t i1, size_t di) {
			    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
			    tptr<T> w = tptr<T>(new T[nfeatures]());
					T loss_local = 0.0;

			    for (size_t i = i0; i < i1; i += di) {
			    	const size_t j0 = ri_val[0][i];
	          if (j0 != RowIndex::NA && !ISNA<U>(data_val[0][j0])) {
	            hash_row(x, hashers_val, i);
	            for (size_t k = 0; k < dt_y_val->ncols; ++k) {
	              const size_t j = ri_val[k][i];
	              T p = link(predict_row(x, w, k));
	              loss_local += loss(p, data_val[k][j]);
	            }
	          }
			    }
			    #pragma omp atomic
			    loss_global += loss_local;
			  },
			  dt_X_val->nrows
			);

		  // If loss dp not decrease, do early stopping.
			loss_global /= (dt_X_val->nrows * dt_y_val->ncols);
			T loss_diff = loss_global_prev - loss_global;
			if (c && loss_diff <= std::numeric_limits<T>::epsilon()) break;
			// If loss decreases, save loss and continue training.
			loss_global_prev = loss_global;
			loss_global = 0;
		}
	}
  double epoch = static_cast<double>(chunk_end) / dt_X->nrows;
	return epoch;
}


/*
*  Make a prediction for an array of hashed features.
*/
template <typename T>
T FtrlReal<T>::predict_row(const uint64ptr& x, tptr<T>& w, size_t k) {
  auto fi = static_cast<T*>(dt_fi->columns[1]->data_w());
  T zero = static_cast<T>(0.0);
  T wTx = zero;
  T l1 = lambda1;
  T ia = 1 / alpha;
  T rr = beta * ia + lambda2;
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T absw = std::max(std::abs(z[k][j]) - l1, zero) / (std::sqrt(n[k][j]) * ia + rr);
    w[i] = -std::copysign(absw, z[k][j]);
    wTx += w[i];
    #pragma omp atomic
    fi[i] += absw; // FIXME: Remove for prediction.
  }
  return wTx;
}


/*
*  Update weights based on a prediction p and the actual target y.
*/
template <typename T>
template <typename U /* column data type */>
void FtrlReal<T>::update(const uint64ptr& x, const tptr<T>& w, T p, U y, size_t k) {
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


/*
*  Predict on a datatable and return a new datatable with
*  the predicted probabilities.
*/
template <typename T>
dtptr FtrlReal<T>::predict(const DataTable* dt_X) {
  if (!model_trained) {
    throw ValueError() << "To make predictions, the model should be trained "
                          "first";
  }
  init_weights();
  is_dt_valid(dt_fi, nfeatures, 2)? init_fi() : create_fi(dt_X); //FIXME: this should be removed

  // Create hashers
  auto hashers = create_hashers(dt_X);

  // Create datatable for predictions and obtain column data pointers
  size_t nlabels = labels.size();
  dtptr dt_p = create_dt_p(dt_X->nrows);
  std::vector<T*> d_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    d_p[i] = static_cast<T*>(dt_p->columns[i]->data_w());
  }


  // Determine which link function we should use.
  T (*f)(T);
  switch (model_type) {
    case FtrlModelType::REGRESSION  : f = identity<T>; break;
    case FtrlModelType::BINOMIAL    : f = sigmoid<T>; break;
    case FtrlModelType::MULTINOMIAL : (nlabels == 2)? f = sigmoid<T> : f = std::exp; break;
    // If this error is thrown, it means that `fit()` and `model_type`
    // went out of sync, so there is a bug in the code.
    default : throw ValueError() << "Cannot make any predictions, "
                                 << "the model was trained in an unknown mode";
  }

	dt::run_interleaved(
		[&](size_t i0, size_t i1, size_t di) {

		  uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
			tptr<T> w = tptr<T>(new T[nfeatures]);

			for (size_t i = i0; i < i1; i += di) {
				hash_row(x, hashers, i);
				for (size_t k = 0; k < nlabels; ++k) {
				  d_p[k][i] = f(predict_row(x, w, k));
				}
			}

		},
		dt_X->nrows
	);

  // For multinomial case, when there is more than two labels,
  // apply `softmax` function. NB: we already applied `std::exp`
  // function to predictions, so only need to normalize probabilities here.
  if (nlabels > 2) normalize_rows(dt_p);

  return dt_p;
}


template <typename T>
void FtrlReal<T>::normalize_rows(dtptr& dt) {
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;

  std::vector<T*> d_cs(ncols);
  for (size_t j = 0; j < ncols; ++j) {
    d_cs[j] = static_cast<T*>(dt->columns[j]->data_w());
  }

  dt::run_interleaved(
    [&](size_t i0, size_t i1, size_t di) {

      for (size_t i = i0; i < i1; i += di) {
			  T denom = static_cast<T>(0.0);
			  for (size_t j = 0; j < ncols; ++j) {
			    denom += d_cs[j][i];
			  }
			  for (size_t j = 0; j < ncols; ++j) {
			    d_cs[j][i] = d_cs[j][i] / denom;
   			}
	    }

    },
    nrows
  );
}


template <typename T>
void FtrlReal<T>::create_model() {
  size_t nlabels = labels.size();
  if (nlabels == 0) ValueError() << "Model cannot have zero labels";

  size_t ncols = 2 * nlabels;
  colvec cols(ncols);
  for (size_t i = 0; i < ncols; ++i) {
    cols[i] = Column::new_data_column(stype<T>::get_stype(), nbins);
  }
  dt_model = dtptr(new DataTable(std::move(cols)));
  init_model();
}


template <typename T>
void FtrlReal<T>::adjust_model() {
  size_t ncols_model = dt_model->ncols;
  size_t ncols_model_new = 2 * labels.size();
  xassert(ncols_model_new > ncols_model)

  colvec cols(ncols_model_new);
  for (size_t i = 0; i < ncols_model; ++i) {
    cols[i] = dt_model->columns[i]->shallowcopy();
  }

  for (size_t i = ncols_model; i < ncols_model_new; ++i) {
    cols[i] = dt_model->columns[i % 2]->shallowcopy();
  }
  dt_model = dtptr(new DataTable(std::move(cols)));
}


template <typename T>
dtptr FtrlReal<T>::create_dt_p(size_t nrows) {
  size_t nlabels = labels.size();
  if (nlabels == 0) ValueError() << "Prediction frame cannot have zero labels";

  colvec cols_p(nlabels);
  for (size_t i = 0; i < nlabels; ++i) {
    cols_p[i] = Column::new_data_column(stype<T>::get_stype(), nrows);
  }

  dtptr dt_p = dtptr(new DataTable(std::move(cols_p), labels));
  return dt_p;
}


template <typename T>
void FtrlReal<T>::reset() {
  dt_model = nullptr;
  dt_fi = nullptr;
  model_trained = false;
  model_type = FtrlModelType::NONE;
  labels.clear();
}


template <typename T>
void FtrlReal<T>::init_model() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols; ++i) {
    auto d_ci = static_cast<T*>(dt_model->columns[i]->data_w());
    std::memset(d_ci, 0, nbins * sizeof(T));
  }
}


template <typename T>
void FtrlReal<T>::init_weights() {
  size_t model_ncols = dt_model->ncols;
  xassert(model_ncols % 2 == 0);
  size_t nlabels = model_ncols / 2;

  z.clear(); z.reserve(nlabels);
  n.clear(); n.reserve(nlabels);
  for (size_t k = 0; k < nlabels; ++k) {
    z.push_back(static_cast<T*>(dt_model->columns[2*k]->data_w()));
    n.push_back(static_cast<T*>(dt_model->columns[2*k + 1]->data_w()));
  }
}


template <typename T>
void FtrlReal<T>::create_fi(const DataTable* dt_X) {
  const strvec& col_names = dt_X->get_names();

  dt::fixed_height_string_col c_fi_names(nfeatures);
  dt::fixed_height_string_col::buffer sb(c_fi_names);
  sb.commit_and_start_new_chunk(0);
  for (const auto& feature_name : col_names) {
    sb.write(feature_name);
  }

  if (interactions) {
    for (size_t i = 0; i < dt_X_ncols - 1; ++i) {
      for (size_t j = i + 1; j < dt_X_ncols; ++j) {
        std::string feature_name = col_names[i] + ":" + col_names[j];
        sb.write(feature_name);
      }
    }
  }

  sb.order();
  sb.commit_and_start_new_chunk(nfeatures);

  Column* c_fi_values = Column::new_data_column(stype<T>::get_stype(), nfeatures);
  dt_fi = dtptr(new DataTable({std::move(c_fi_names).to_column(), c_fi_values},
                              {"feature_name", "feature_importance"})
                             );
  init_fi();
}


template <typename T>
void FtrlReal<T>::init_fi() {
  if (dt_fi == nullptr) return;
  auto d_fi = static_cast<T*>(dt_fi->columns[1]->data_w());
  std::memset(d_fi, 0, nfeatures * sizeof(T));
}


template <typename T>
void FtrlReal<T>::define_features(size_t ncols_in) {
  dt_X_ncols = ncols_in;
  size_t n_inter_features = (interactions)? dt_X_ncols * (dt_X_ncols - 1) / 2 : 0;
  nfeatures = dt_X_ncols + n_inter_features;
}


template <typename T>
std::vector<hasherptr> FtrlReal<T>::create_hashers(const DataTable* dt) {
  std::vector<hasherptr> hashers;
  hashers.clear();
  hashers.reserve(dt_X_ncols);

  for (size_t i = 0; i < dt_X_ncols; ++i) {
    Column* col = dt->columns[i];
    hashers.push_back(create_hasher(col));
  }

  // Also, pre-hash column names.
  // TODO: if we stick to default column names, like `C*`,
  // this may only be necessary, when number of features increases.
  const std::vector<std::string>& c_names = dt->get_names();
  colnames_hashes.clear();
  colnames_hashes.reserve(dt_X_ncols);
  for (size_t i = 0; i < dt_X_ncols; i++) {
    uint64_t h = hash_murmur2(c_names[i].c_str(),
                             c_names[i].length() * sizeof(char),
                             0);
    colnames_hashes.push_back(h);
  }
  return hashers;
}


template <typename T>
hasherptr FtrlReal<T>::create_hasher(const Column* col) {
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    return hasherptr(new HasherBool(col));
    case SType::INT8:    return hasherptr(new HasherInt<int8_t>(col));
    case SType::INT16:   return hasherptr(new HasherInt<int16_t>(col));
    case SType::INT32:   return hasherptr(new HasherInt<int32_t>(col));
    case SType::INT64:   return hasherptr(new HasherInt<int64_t>(col));
    case SType::FLOAT32: return hasherptr(new HasherFloat<float>(col));
    case SType::FLOAT64: return hasherptr(new HasherFloat<double>(col));
    case SType::STR32:   return hasherptr(new HasherString<uint32_t>(col));
    case SType::STR64:   return hasherptr(new HasherString<uint64_t>(col));
    default:             throw TypeError() << "Cannot hash a column of type "
                                           << stype;
  }
}


/*
*  Hash each element of the datatable row, do feature interactions if requested.
*/
template <typename T>
void FtrlReal<T>::hash_row(uint64ptr& x, std::vector<hasherptr>& hashers, size_t row) {
  // Hash column values adding a column name hash, so that the same value
  // in different columns results in different hashes.
  for (size_t i = 0; i < dt_X_ncols; ++i) {
    x[i] = (hashers[i]->hash(row) + colnames_hashes[i]) % nbins;
  }

  // Do feature interactions if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach here.
  if (interactions) {
    size_t count = 0;
    for (size_t i = 0; i < dt_X_ncols - 1; ++i) {
      for (size_t j = i + 1; j < dt_X_ncols; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_murmur2(s.c_str(), s.length() * sizeof(char), 0);
        x[dt_X_ncols + count] = h % nbins;
        count++;
      }
    }
  }
}


template <typename T>
bool FtrlReal<T>::is_dt_valid(const dtptr& dt, size_t nrows_in, size_t ncols_in) {
  if (dt == nullptr) return false;
  // Normally, this exception should never be thrown.
  // For the moment, the only purpose of this check is to make sure we didn't
  // do something wrong to the model and feature importance datatables.
  if (dt->ncols != ncols_in) {
    throw ValueError() << "Datatable should have " << ncols_in << " column"
                       << (ncols_in == 1? "" : "s") << ", got: " << dt->ncols;
  }
  if (dt->nrows != nrows_in) return false;
  return true;
}


template <typename T>
bool FtrlReal<T>::is_trained() {
  return model_trained;
}


/*
*  Get a shallow copy of a model if available.
*/
template <typename T>
DataTable* FtrlReal<T>::get_model() {
  if (dt_model == nullptr) return nullptr;
  return dt_model->copy();
}


template <typename T>
FtrlModelType FtrlReal<T>::get_model_type() {
  return model_type;
}



/*
* Normalize a column of feature importances to [0; 1]
* This column has only positive values, so we simply divide its
* content by the maximum. Another option is to do min-max normalization,
* but this may lead to some features having zero importance,
* while in reality they don't.
*/
template <typename T>
DataTable* FtrlReal<T>::get_fi(bool normalize /* = true */) {
  if (dt_fi == nullptr) return nullptr;

  DataTable* dt_fi_copy = dt_fi->copy();
  if (normalize) {
    auto c_fi = static_cast<RealColumn<T>*>(dt_fi_copy->columns[1]);
    T max = c_fi->max();
    T epsilon = std::numeric_limits<T>::epsilon();
    T* d_fi = c_fi->elements_w();

    T norm_factor = static_cast<T>(1.0);
    if (fabs(max) > epsilon) norm_factor /= max;

    for (size_t i = 0; i < c_fi->nrows; ++i) {
      d_fi[i] *= norm_factor;
    }
    c_fi->get_stats()->reset();
  }
  return dt_fi_copy;
}


/*
*  Other getters and setters.
*  Here we assume that all the validation for setters is done in `py_ftrl.cc`.
*/
template <typename T>
std::vector<uint64_t> FtrlReal<T>::get_colnames_hashes() {
  return colnames_hashes;
}


template <typename T>
size_t FtrlReal<T>::get_dt_X_ncols() {
  return dt_X_ncols;
}


template <typename T>
size_t FtrlReal<T>::get_nfeatures() {
  return nfeatures;
}


template <typename T>
double FtrlReal<T>::get_alpha() {
  return params.alpha;
}


template <typename T>
double FtrlReal<T>::get_beta() {
  return params.beta;
}


template <typename T>
double FtrlReal<T>::get_lambda1() {
  return params.lambda1;
}


template <typename T>
double FtrlReal<T>::get_lambda2() {
  return params.lambda2;
}


template <typename T>
uint64_t FtrlReal<T>::get_nbins() {
  return params.nbins;
}


template <typename T>
bool FtrlReal<T>::get_interactions() {
  return params.interactions;
}


template <typename T>
size_t FtrlReal<T>::get_nepochs() {
  return params.nepochs;
}


template <typename T>
bool FtrlReal<T>::get_double_precision() {
  return params.double_precision;
}


template <typename T>
FtrlParams FtrlReal<T>::get_params() {
  return params;
}


template <typename T>
strvec FtrlReal<T>::get_labels() {
  return labels;
}


/*
*  Set a model, assuming all the validation is done in `py_ftrl.cc`
*/
template <typename T>
void FtrlReal<T>::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_nbins(dt_model->nrows);
  dt_X_ncols = 0;
  nfeatures = 0;
  model_trained = true;

  //FIXME: remove, because we are not sure about the model_type
  strvec labels_in = {"C0"};
  set_labels(labels_in);
}


template <typename T>
void FtrlReal<T>::set_model_type(FtrlModelType model_type_in) {
  model_type = model_type_in;
}


template <typename T>
void FtrlReal<T>::set_fi(DataTable* dt_fi_in) {
  dt_fi = dtptr(dt_fi_in->copy());
  nfeatures = dt_fi->nrows;
}


template <typename T>
void FtrlReal<T>::set_alpha(double alpha_in) {
  params.alpha = alpha_in;
  alpha = static_cast<T>(alpha_in);
}


template <typename T>
void FtrlReal<T>::set_beta(double beta_in) {
  params.beta = beta_in;
  beta = static_cast<T>(beta_in);
}


template <typename T>
void FtrlReal<T>::set_lambda1(double lambda1_in) {
  params.lambda1 = lambda1_in;
  lambda1 = static_cast<T>(lambda1_in);
}


template <typename T>
void FtrlReal<T>::set_lambda2(double lambda2_in) {
  params.lambda2 = lambda2_in;
  lambda2 = static_cast<T>(lambda2_in);
}


template <typename T>
void FtrlReal<T>::set_nbins(uint64_t nbins_in) {
  params.nbins = nbins_in;
  nbins = nbins_in;
}


template <typename T>
void FtrlReal<T>::set_interactions(bool interactions_in) {
  params.interactions = interactions_in;
  interactions = interactions_in;
}


template <typename T>
void FtrlReal<T>::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
  nepochs = nepochs_in;
}

template <typename T>
void FtrlReal<T>::set_double_precision(bool double_precision_in) {
  params.double_precision = double_precision_in;
  double_precision = double_precision_in;
}


template <typename T>
void FtrlReal<T>::set_labels(strvec labels_in) {
  labels = labels_in;
}


} // namespace dt

#endif
