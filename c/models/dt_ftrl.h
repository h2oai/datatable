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
#ifndef dt_MODELS_FTRL_h
#define dt_MODELS_FTRL_h
#include "py_datatable.h"
#include "str/py_str.h"
#include "utils/parallel.h"
#include "models/hash.h"
#include "models/utils.h"


using hashptr = std::unique_ptr<Hash>;
enum class RegType : uint8_t {
  NONE        = 0,
  REGRESSION  = 1,
  BINOMIAL    = 2,
  MULTINOMIAL = 3
};

namespace dt {

struct FtrlParams {
    double alpha;
    double beta;
    double lambda1;
    double lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;
    bool double_precision;
    size_t: 48;
    FtrlParams() : alpha(0.005), beta(1.0), lambda1(0.0), lambda2(1.0),
                   nbins(1000000), nepochs(1), interactions(false),
                   double_precision(false) {}
};


class FtrlBase {
  public:
    virtual ~FtrlBase();
    virtual void dispatch_fit(const DataTable*, const DataTable*) = 0;
    virtual dtptr predict(const DataTable*) = 0;
    virtual bool is_trained() = 0;
    virtual void reset() = 0;

    // Getters
    virtual DataTable* get_model() = 0;
    virtual DataTable* get_fi(bool normaliza = true) = 0;
    virtual size_t get_nfeatures() = 0;
    virtual size_t get_dt_X_ncols() = 0;
    virtual std::vector<uint64_t> get_colnames_hashes() = 0;
    virtual double get_alpha() = 0;
    virtual double get_beta() = 0;
    virtual double get_lambda1() = 0;
    virtual double get_lambda2() = 0;
    virtual uint64_t get_nbins() = 0;
    virtual size_t get_nepochs() = 0;
    virtual bool get_interactions() = 0;
    virtual bool get_double_precision() = 0;
    virtual FtrlParams get_params() = 0;

    // Setters
    virtual void set_model(DataTable*) = 0;
    virtual void set_fi(DataTable*) = 0;
    virtual void set_alpha(double) = 0;
    virtual void set_beta(double) = 0;
    virtual void set_lambda1(double) = 0;
    virtual void set_lambda2(double) = 0;
    virtual void set_nbins(uint64_t) = 0;
    virtual void set_nepochs(size_t) = 0;
    virtual void set_interactions(bool) = 0;
    virtual void set_double_precision(bool) = 0;
};


template <typename T /* float or double */>
class Ftrl : public dt::FtrlBase{
  private:
    // Model datatable with the shape of (nbins, 2 * nlabels).
    dtptr dt_model;

    // Feature importances datatable with the shape of (nfeatures, 2),
    // where first column contains feature names and the second one
    // feature importance values.
    // FIXME: make dt_fi thread private.
    dtptr dt_fi;

    // FTRL fitting parameters
    FtrlParams params;
    T alpha;
    T beta;
    T lambda1;
    T lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;
    bool double_precision;

    // Number of columns in a fitting datatable and a total number of features
    size_t dt_X_ncols;

    size_t nfeatures;
    strvec labels;

    // Whether or not the FTRL model was trained, `false` by default.
    // `fit(...)` and `set_model(...)` methods will set this to `true`.
    bool model_trained;
    RegType reg_type;


    // Column hashers and a vector of hashed column names
    std::vector<hashptr> hashers;
    std::vector<uint64_t> colnames_hashes;

  public:
    Ftrl(FtrlParams);
    static const std::vector<std::string> model_colnames;

    // Fit dispatcher and fitting methods
    void dispatch_fit(const DataTable*, const DataTable*) override;
    template <typename U, typename F>
    void fit(const DataTable*, const DataTable*, F);
    template <typename U>
    void fit_regression(const DataTable*, const DataTable*);
    void fit_binomial(const DataTable*, const DataTable*);
    void fit_multinomial(const DataTable*, const DataTable*);


    // Predicting method
    dtptr predict(const DataTable*) override;
    T predict_row(const uint64ptr&, tptr<T>&, size_t);
    void update(const uint64ptr&, tptr<T>&, T, T, size_t);
    dtptr create_p(size_t);

    // Model and feature importance handling methods
    void create_model();
    void reset() override;
    void init_model();
    void create_fi(const DataTable*);
    void init_fi();
    void define_features(size_t);
    void normalize_rows(dtptr&);

    // Hashing methods
    void create_hashers(const DataTable*);
    static hashptr create_colhasher(const Column*);
    void hash_row(uint64ptr&, size_t);

    // Model validation methods
    static bool is_dt_valid(const dtptr&, size_t, size_t);
    bool is_trained() override;

    // Getters
    DataTable* get_model() override;
    DataTable* get_fi(bool normalize = true) override;
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

    // Setters
    void set_model(DataTable*) override;
    void set_fi(DataTable*) override;
    void set_alpha(double) override;
    void set_beta(double) override;
    void set_lambda1(double) override;
    void set_lambda2(double) override;
    void set_nbins(uint64_t) override;
    void set_nepochs(size_t) override;
    void set_interactions(bool) override;
    void set_double_precision(bool) override;
};



/*
*  Set up FTRL parameters and initialize weights.
*/
template <typename T>
Ftrl<T>::Ftrl(FtrlParams params_in) :
  params(params_in),
  alpha(static_cast<T>(params_in.alpha)),
  beta(static_cast<T>(params_in.beta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nbins(params_in.nbins),
  nepochs(params_in.nepochs),
  interactions(params_in.interactions),
  dt_X_ncols(0),
  nfeatures(0),
  model_trained(false),
  reg_type(RegType::NONE)
{
}


/*
*  Make an appropriate fit call depending on a target column type.
*/
template <typename T>
void Ftrl<T>::dispatch_fit(const DataTable* dt_X, const DataTable* dt_y) {
  SType stype_y = dt_y->columns[0]->stype();
  switch (stype_y) {
    case SType::BOOL:    fit_binomial(dt_X, dt_y); break;
    case SType::INT8:    fit_regression<int8_t>(dt_X, dt_y); break;
    case SType::INT16:   fit_regression<int16_t>(dt_X, dt_y); break;
    case SType::INT32:   fit_regression<int32_t>(dt_X, dt_y); break;
    case SType::INT64:   fit_regression<int64_t>(dt_X, dt_y); break;
    case SType::FLOAT32: fit_regression<float>(dt_X, dt_y); break;
    case SType::FLOAT64: fit_regression<double>(dt_X, dt_y); break;
    case SType::STR32:   [[clang::fallthrough]];
    case SType::STR64:   fit_multinomial(dt_X, dt_y); break;
    default:             throw TypeError() << "Cannot predict for a column "
                                           << "of type `" << stype_y << "`";
  }
  model_trained = true;
}



/*
*  Fit FTRL model on a datatable.
*/
template <typename T>
template <typename U /* column data type */, typename F /* link function */>
void Ftrl<T>::fit(const DataTable* dt_X, const DataTable* dt_y, F f) {
  define_features(dt_X->ncols);
  labels = dt_y->get_names();
  size_t nlabels = labels.size();

  // Create model and feature importances datatables if needed
  if (!is_dt_valid(dt_model, nbins, 2 * nlabels)) create_model();
  if (!is_dt_valid(dt_fi, nfeatures, 2)) create_fi(dt_X);

  // Create column hashers
  create_hashers(dt_X);


  // Do training for `nepochs`.
  for (size_t e = 0; e < nepochs; ++e) {
    #pragma omp parallel num_threads(config::nthreads)
    {
      // Arrays to store hashed features and their correspondent weights.
      uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
      tptr<T> w = tptr<T>(new T[nfeatures]);

      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());

      auto c_y0 = dt_y->columns[0];
      auto d_y0 = static_cast<const U*>(c_y0->data());
      auto& ri_y0 = c_y0->rowindex();

      for (size_t i = ith; i < dt_X->nrows; i += nth) {
          const size_t j0 = ri_y0[i];
          // Note that for RegType::BINOMIAL and RegType::REGRESSION
          // dt_y has only one column that may contain NA's or be a view
          // with an NA rowindex. For RegType::MULTINOMIAL we have as many
          // columns as there are labels, and split_into_nhot() filters out
          // NA's and can never be a view. Therefore, to ignore NA targets
          // it is enough to check the condition below for the zero column only.
          // FIXME: this condition can be removed for RegType::MULTINOMIAL.
          if (j0 != RowIndex::NA && !ISNA<U>(d_y0[j0])) {
            hash_row(x, i);
            for (size_t k = 0; k < dt_y->ncols; ++k) {
              auto& ri_y = dt_y->columns[k]->rowindex();
              auto d_y = static_cast<const U*>(dt_y->columns[k]->data());
              const size_t j = ri_y[i];
              T p = f(predict_row(x, w, k));
              T y = static_cast<T>(d_y[j]);
              update(x, w, p, y, k);
            }
          }
      }
    }
  }
}


template <typename T>
void Ftrl<T>::fit_binomial(const DataTable* dt_X, const DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::BINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from binomial. To train it "
                         "in a binomial mode this model should be reset.";
  }
  reg_type = RegType::BINOMIAL;
  fit<int8_t>(dt_X, dt_y, sigmoid<T>);
}


template <typename T>
template <typename U>
void Ftrl<T>::fit_regression(const DataTable* dt_X, const DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::REGRESSION) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from regression. To train it "
                         "in a regression mode this model should be reset.";
  }
  reg_type = RegType::REGRESSION;
  fit<U>(dt_X, dt_y, identity<T>);
}


template <typename T>
void Ftrl<T>::fit_multinomial(const DataTable* dt_X, const DataTable* dt_y) {
  if (reg_type != RegType::NONE && reg_type != RegType::MULTINOMIAL) {
    throw TypeError() << "This model has already been trained in a "
                         "mode different from multinomial. To train it "
                         "in a multinomial mode this model should be reset.";
  }
  reg_type = RegType::MULTINOMIAL;


  // Convert dt_y to one hot encoded dt_y_nhot
  dtptr dt_y_nhot = dtptr(split_into_nhot(dt_y->columns[0], '\0'));

  // FIXME: add a negative column
  fit<int8_t>(dt_X, dt_y_nhot.get(), sigmoid<T>);
}


/*
*  Update weights based on prediction p and the actual target y.
*/
template <typename T>
void Ftrl<T>::update(const uint64ptr& x, tptr<T>& w, T p, T y, size_t k) {
  auto z = static_cast<T*>(dt_model->columns[k]->data_w());
  auto n = static_cast<T*>(dt_model->columns[k+1]->data_w());
  T ia = 1 / alpha;
  T g = p - y;
  T gsq = g * g;

  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T sigma = (std::sqrt(n[j] + gsq) - std::sqrt(n[j])) * ia;
    z[j] += g - sigma * w[i];
    n[j] += gsq;
  }
}


/*
*  Make predictions for a test datatable and return targets as a new datatable.
*/
template <typename T>
dtptr Ftrl<T>::predict(const DataTable* dt_X) {
  xassert(model_trained);
  is_dt_valid(dt_fi, nfeatures, 2)? init_fi() : create_fi(dt_X);

  // Re-create hashers as `stype`s for prediction may be different from
  // those used for fitting
  create_hashers(dt_X);

  // Create a datatable for predictions.
  size_t nlabels = labels.size();
  dtptr dt_p = create_p(dt_X->nrows);


  // Determine which link function we should use.
  T (*f)(T);
  switch (reg_type) {
    case RegType::REGRESSION  : f = identity<T>; break;
    case RegType::BINOMIAL    : f = sigmoid<T>; break;
    case RegType::MULTINOMIAL : (nlabels == 2)? f = sigmoid<T> : f = std::exp; break;
    // If this error is thrown, it means that `fit()` and `reg_type`
    // went out of sync, so there is a bug in the code.
    default : throw ValueError() << "Cannot make any predictions, "
                                 << "the model was trained in an unknown mode";
  }


  #pragma omp parallel num_threads(config::nthreads)
  {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    tptr<T> w = tptr<T>(new T[nfeatures]);

    size_t ith = static_cast<size_t>(omp_get_thread_num());
    size_t nth = static_cast<size_t>(omp_get_num_threads());

    for (size_t i = ith; i < dt_X->nrows; i += nth) {
      hash_row(x, i);
      for (size_t k = 0; k < nlabels; ++k) {
        auto d_p = static_cast<T*>(dt_p->columns[k]->data_w());
        d_p[i] = f(predict_row(x, w, k));
      }
    }
  }

  // For multinomial case, when there is more than two labels,
  // apply `softmax` function. NB: we already applied `std::exp`
  // function to predictions, so only need to normalize probabilities here.
  if (nlabels > 2) normalize_rows(dt_p);

  return dt_p;
}


template <typename T>
void Ftrl<T>::normalize_rows(dtptr& dt) {
  size_t nrows = dt->nrows;
  size_t ncols = dt->ncols;

  std::vector<T*> d_cs;
  d_cs.reserve(ncols);
  for (size_t j = 0; j < ncols; ++j) {
    auto d_c = static_cast<T*>(dt->columns[j]->data_w());
    d_cs.push_back(d_c);
  }

  #pragma omp parallel for num_threads(config::nthreads)
  for (size_t i = 0; i < nrows; ++i) {
    T denom = static_cast<T>(0.0);
    for (size_t j = 0; j < ncols; ++j) {
      denom += d_cs[j][i];
    }
    for (size_t j = 0; j < ncols; ++j) {
      d_cs[j][i] = d_cs[j][i] / denom;
    }
  }
}


/*
*  Set column names for `dt_model` and default parameter values.
*/
template <typename T>
const std::vector<std::string> Ftrl<T>::model_colnames = {"z", "n"};



/*
*  Make a prediction for an array of hashed features.
*/
template <typename T>
T Ftrl<T>::predict_row(const uint64ptr& x, tptr<T>& w, size_t k) {
  auto z = static_cast<T*>(dt_model->columns[k]->data_w());
  auto n = static_cast<T*>(dt_model->columns[k+1]->data_w());
  auto fi = static_cast<T*>(dt_fi->columns[1]->data_w());
  T wTx = 0;
  T l1 = lambda1;
  T ia = 1 / alpha;
  T rr = beta * ia + lambda2;
  T zero = static_cast<T>(0.0);
  for (size_t i = 0; i < nfeatures; ++i) {
    size_t j = x[i];
    T absw = std::max(std::abs(z[j]) - l1, zero) / (std::sqrt(n[j]) * ia + rr);
    w[i] = -std::copysign(absw, z[j]);
    wTx += w[i];
    fi[i] += absw; // FIXME: Make fi thread private and do not change for prediction only.
  }
  return wTx;
}


template <typename T>
void Ftrl<T>::create_model() {
  size_t nlabels = labels.size();
  if (nlabels == 0) ValueError() << "Model cannot have zero labels";
  size_t model_ncols = 2 * nlabels;
  colvec cols_model(model_ncols);
  for (size_t i = 0; i < model_ncols; ++i) {
    cols_model[i] = Column::new_data_column(stype<T>::get_stype(), nbins);
  }
  dt_model = dtptr(new DataTable(std::move(cols_model)));
  init_model();
}


template <typename T>
dtptr Ftrl<T>::create_p(size_t nrows) {
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
void Ftrl<T>::reset() {
  dt_model = nullptr;
  dt_fi = nullptr;
  model_trained = false;
  reg_type = RegType::NONE;
}


template <typename T>
void Ftrl<T>::init_model() {
  if (dt_model == nullptr) return;
  for (size_t i = 0; i < dt_model->ncols; ++i) {
    auto d_ci = static_cast<T*>(dt_model->columns[i]->data_w());
    std::memset(d_ci, 0, nbins * sizeof(T));
  }
}


// FIXME: convert from py to dt.
template <typename T>
void Ftrl<T>::create_fi(const DataTable* dt_X) {
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
void Ftrl<T>::init_fi() {
  if (dt_fi == nullptr) return;
  auto d_fi = static_cast<T*>(dt_fi->columns[1]->data_w());
  std::memset(d_fi, 0, nfeatures * sizeof(T));
}


template <typename T>
void Ftrl<T>::define_features(size_t ncols_in) {
  dt_X_ncols = ncols_in;
  size_t n_inter_features = (interactions)? dt_X_ncols * (dt_X_ncols - 1) / 2 : 0;
  nfeatures = dt_X_ncols + n_inter_features;
}


template <typename T>
void Ftrl<T>::create_hashers(const DataTable* dt) {
  hashers.clear();
  hashers.reserve(dt_X_ncols);

  for (size_t i = 0; i < dt_X_ncols; ++i) {
    Column* col = dt->columns[i];
    hashers.push_back(create_colhasher(col));
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
}


template <typename T>
hashptr Ftrl<T>::create_colhasher(const Column* col) {
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    return hashptr(new HashBool(col));
    case SType::INT8:    return hashptr(new HashInt<int8_t>(col));
    case SType::INT16:   return hashptr(new HashInt<int16_t>(col));
    case SType::INT32:   return hashptr(new HashInt<int32_t>(col));
    case SType::INT64:   return hashptr(new HashInt<int64_t>(col));
    case SType::FLOAT32: return hashptr(new HashFloat<float>(col));
    case SType::FLOAT64: return hashptr(new HashFloat<double>(col));
    case SType::STR32:   return hashptr(new HashString<uint32_t>(col));
    case SType::STR64:   return hashptr(new HashString<uint64_t>(col));
    default:             throw TypeError() << "Cannot hash a column of type "
                                           << stype;
  }
}


/*
*  Hash each element of the datatable row, do feature interactions if requested.
*/
template <typename T>
void Ftrl<T>::hash_row(uint64ptr& x, size_t row) {
  for (size_t i = 0; i < dt_X_ncols; ++i) {
    // Hash a value adding a column name hash to it, so that the same value
    // in different columns results in different hashes.
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
bool Ftrl<T>::is_dt_valid(const dtptr& dt, size_t nrows_in, size_t ncols_in) {
  if (dt == nullptr) return false;
  // Normally, this exception should never be thrown.
  // For the moment, the only purpose of this check is to make sure we didn't
  // do something wrong to the model and feature importance datatables.
  if (dt->ncols != ncols_in) {
    throw ValueError() << "Datatable should have " << ncols_in << "column"
                       << (ncols_in == 1? "" : "s") << ", got: " << dt->ncols;
  }
  if (dt->nrows != nrows_in) return false;
  return true;
}


template <typename T>
bool Ftrl<T>::is_trained() {
  return model_trained;
}


/*
*  Get a shallow copy of an FTRL model if available.
*/
template <typename T>
DataTable* Ftrl<T>::get_model() {
  if (dt_model != nullptr) {
    return dt_model->copy();
  } else {
    return nullptr;
  }
}


/*
* Normalize a column of feature importances to [0; 1]
* This column has only positive values, so we simply divide its
* content by the maximum. Another option is to do min-max normalization,
* but this may lead to some features having zero importance,
* while in reality they don't.
*/
template <typename T>
DataTable* Ftrl<T>::get_fi(bool normalize /* = true */) {
  if (dt_fi != nullptr) {
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
  } else {
    return nullptr;
  }
}


/*
*  Other getters and setters.
*  Here we assume that all the validation for setters is done in `py_ftrl.cc`.
*/
template <typename T>
std::vector<uint64_t> Ftrl<T>::get_colnames_hashes() {
  return colnames_hashes;
}


template <typename T>
size_t Ftrl<T>::get_dt_X_ncols() {
  return dt_X_ncols;
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
bool Ftrl<T>::get_interactions() {
  return params.interactions;
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
FtrlParams Ftrl<T>::get_params() {
  return params;
}


/*
*  Set an FTRL model, assuming all the validation is done in `py_ftrl.cc`
*/
template <typename T>
void Ftrl<T>::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_nbins(dt_model->nrows);
  dt_X_ncols = 0;
  nfeatures = 0;
  model_trained = true;
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
void Ftrl<T>::set_interactions(bool interactions_in) {
  params.interactions = interactions_in;
  interactions = interactions_in;
}


template <typename T>
void Ftrl<T>::set_nepochs(size_t nepochs_in) {
  params.nepochs = nepochs_in;
  nepochs = nepochs_in;
}

template <typename T>
void Ftrl<T>::set_double_precision(bool double_precision_in) {
  params.double_precision = double_precision_in;
  double_precision = double_precision_in;
}

} // namespace dt

#endif
