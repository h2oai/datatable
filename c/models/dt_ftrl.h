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
#include "utils/parallel.h"
#include "models/hash.h"
#include "models/utils.h"


using hashptr = std::unique_ptr<Hash>;

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
                   nbins(1000000), nepohcs(1), interactions(false),
                   double_precision(false) {}
};


class FtrlBase {
  public:
    virtual ~FtrlBase() = 0;
    virtual void fit(const DataTable*, const DataTable*) = 0;
    virtual dtptr predict(const DataTable*) = 0;
    virtual bool is_trained() = 0;

    // Getters
    virtual DataTable* get_model() = 0;
    virtual DataTable* get_fi() = 0;
    virtual size_t get_nfeatures() = 0;
    virtual size_t get_ncols() = 0;
    virtual std::vector<uint64_t> get_colnames_hashes() = 0;
    virtual double get_alpha() = 0;
    virtual double get_beta() = 0;
    virtual double get_lambda1() = 0;
    virtual double get_lambda2() = 0;
    virtual uint64_t get_nbins() = 0;
    virtual size_t get_nepochs() = 0;
    virtual bool get_interactions() = 0;
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
};


template <typename T /* float or double */>
class Ftrl {
  private:
    // Model datatable and column data pointers
    dtptr dt_model;
    T* z;
    T* n;

    // Feature importance datatable and a column data pointer
    dtptr dt_fi;
    T* fi;

    // FTRL fitting parameters
    FtrlParams params;
    T alpha;
    T beta;
    T lambda1;
    T lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;

    // Number of columns in a fitting datatable and a total number of features
    size_t ncols;
    size_t nfeatures;

    // Whether or not the FTRL model was trained, `false` by default.
    // `fit(...)` and `set_model(...)` methods will set this to `true`.
    bool model_trained;
    size_t : 56;

    // Column hashers and a vector of hashed column names
    std::vector<hashptr> hashers;
    std::vector<uint64_t> colnames_hashes;

  public:
    Ftrl(FtrlParams);
    static const std::vector<std::string> model_colnames;

    // Fitting and predicting methods
    template <typename U, typename F>
    void fit(const DataTable*, const Column*, F);
    template<typename F> dtptr predict(const DataTable*, F f);
    T predict_row(const uint64ptr&, tptr<T>&);
    void update(const uint64ptr&, tptr<T>&, T, T);

    // Model and feature importance handling methods
    void create_model();
    void reset_model();
    void init_weights();
    void create_fi(DataTable*);
    void reset_fi();
    void init_fi();
    void define_features(size_t);

    // Hashing methods
    void create_hashers(const DataTable*);
    static hashptr create_colhasher(const Column*);
    void hash_row(uint64ptr&, size_t);

    // Model validation methods
    static bool is_dt_valid(const dtptr&t, size_t, size_t);
    bool is_trained();

    // Getters
    DataTable* get_model();
    DataTable* get_fi();
    size_t get_nfeatures();
    size_t get_ncols();
    std::vector<uint64_t> get_colnames_hashes();
    double get_alpha();
    double get_beta();
    double get_lambda1();
    double get_lambda2();
    uint64_t get_nbins();
    size_t get_nepochs();
    bool get_interactions();
    FtrlParams get_params();

    // Setters
    void set_model(DataTable*);
    void set_fi(DataTable*);
    void set_alpha(double);
    void set_beta(double);
    void set_lambda1(double);
    void set_lambda2(double);
    void set_nbins(uint64_t);
    void set_nepochs(size_t);
    void set_interactions(bool);
};



/*
*  Set up FTRL parameters and initialize weights.
*/
template <typename T>
Ftrl<T>::Ftrl(FtrlParams params_in) :
  z(nullptr),
  n(nullptr),
  fi(nullptr),
  params(params_in),
  alpha(static_cast<T>(params_in.alpha)),
  beta(static_cast<T>(params_in.beta)),
  lambda1(static_cast<T>(params_in.lambda1)),
  lambda2(static_cast<T>(params_in.lambda2)),
  nbins(params_in.nbins),
  nepochs(params_in.nepochs),
  interactions(params_in.interactions),
  ncols(0),
  nfeatures(0),
  model_trained(false)
{
}


/*
*  Make an appropriate fit call depending on a target column type.
*/
template <typename T>
void Ftrl<T>::fit(const DataTable* dt_X, const DataTable* dt_y) {
  define_features(dt_X->ncols);
  is_dt_valid(dt_model, nbins, 2)? init_weights() : create_model();
  is_dt_valid(dt_fi, nfeatures, 2)? init_fi() : create_fi(dt_X);

  // Create column hashers
  create_hashers(dt_X);

  SType stype_y = dt_y->columns[0]->stype();
  switch (stype_y) {
    case SType::BOOL:    fit_binomial(dt_X, dt_y, sigmoid<T>); break;
    case SType::INT8:    fit_binomial<int8_t>(dt_X, dt_y, identity<T>); break;
    case SType::INT16:   fit_binomial<int16_t>(dt_X, dt_y, identity<T>); break;
    case SType::INT32:   fit_binomial<int32_t>(dt_X, dt_y, identity<T>); break;
    case SType::INT64:   fit_binomial<int64_t>(dt_X, dt_y, identity<T>); break;
    case SType::FLOAT32: fit_binomial<float>(dt_X, dt_y, identity<T>); break;
    case SType::FLOAT64: fit_binomial<double>(dt_X, dt_y, identity<T>); break;
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
void Ftrl<T>::fit_binomial(const DataTable* dt_X, const DataTable* dt_y, F f) {
  // Get the target column
  auto d_y = static_cast<const U*>(c_y->data());
  const RowIndex ri_y = c_y->rowindex();

  // Do training for `nepochs`.
  for (size_t e = 0; e < nepochs; ++e) {
    #pragma omp parallel num_threads(config::nthreads)
    {
      // Arrays to store hashed features and their correspondent weights.
      uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
      tptr<T> w = tptr<T>(new T[nfeatures]);

      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());

      for (size_t i = ith; i < dt_X->nrows; i += nth) {
          size_t j = ri_y[i];
          if (j != RowIndex::NA && !ISNA<U>(d_y[j])) {
            hash_row(x, i);
            T p = f(predict_row(x, w));
            T y = static_cast<T>(d_y[j]);
            update(x, w, p, y);
          }
      }
    }
  }
}


/*
*  Update weights based on prediction `p` and the actual target `y`.
*/
template <typename T>
void Ftrl<T>::update(const uint64ptr& x, tptr<T>& w, T p, T y) {
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
template<typename F>
dtptr Ftrl<T>::predict(const DataTable* dt_X, F f) {
  xassert(model_trained);
  define_features(dt_X->ncols);
  init_weights();
  is_dt_valid(dt_fi, nfeatures, 1)? init_fi() : create_fi(dt_X);

  // Re-create hashers as `stype`s for prediction may be different from
  // those used for fitting
  create_hashers(dt_X);

  // Create a datatable to store targets
  dtptr dt_y;
  Column* col_target = Column::new_data_column(stype<T>::get_stype(), dt_X->nrows);
  dt_y = dtptr(new DataTable({col_target}, {"target"}));
  auto d_y = static_cast<T*>(dt_y->columns[0]->data_w());

  #pragma omp parallel num_threads(config::nthreads)
  {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    tptr<T> w = tptr<T>(new T[nfeatures]);

    size_t ith = static_cast<size_t>(omp_get_thread_num());
    size_t nth = static_cast<size_t>(omp_get_num_threads());

    for (size_t i = ith; i < dt_X->nrows; i += nth) {
      hash_row(x, i);
      d_y[i] = f(predict_row(x, w));
    }
  }
  return dt_y;
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
T Ftrl<T>::predict_row(const uint64ptr& x, tptr<T>& w) {
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
    fi[i] += absw; // Update feature importance vector
  }
  return wTx;
}


template <typename T>
void Ftrl<T>::create_model() {
  Column* col_z = Column::new_data_column(stype<T>::get_stype(), nbins);
  Column* col_n = Column::new_data_column(stype<T>::get_stype(), nbins);
  dt_model = dtptr(new DataTable({col_z, col_n}, model_colnames));
  reset_model();
}


template <typename T>
void Ftrl<T>::reset_model() {
  init_weights();
  if (z == nullptr || n == nullptr) return;
  std::memset(z, 0, nbins * sizeof(T));
  std::memset(n, 0, nbins * sizeof(T));
  model_trained = false;
}


template <typename T>
void Ftrl<T>::init_weights() {
  if (dt_model == nullptr) return;
  z = static_cast<T*>(dt_model->columns[0]->data_w());
  n = static_cast<T*>(dt_model->columns[1]->data_w());
}



template <typename T>
void Ftrl<T>::create_fi(const DataTable* dt_x) {
  reset_feature_names(); //FIXME
  size_t ncols = dt_X->ncols;
  const std::vector<std::string>& column_names = dt_X->get_names();

  dt::fixed_height_string_col c_fi_names_col(nfeas);
  dt::fixed_height_string_col::buffer sb(fi_names_col);
  sb.commit_and_start_new_chunk(0);
  for (const auto& feature_name : column_names) {
    sb.write(feature_name);
  }

  if (interactions) {
    for (size_t i = 0; i < ncols - 1; ++i) {
      for (size_t j = i + 1; j < ncols; ++j) {
        std::string feature_name = column_names[i] + ":" + column_names[j];
        sb.write(feature_name);
      }
    }
  }

  sb.order();
  sb.commit_and_start_new_chunk(nfeatures);

  Column* c_fi_values = Column::new_data_column(stype<T>::get_stype(), nfeatures);
  dt_fi = dtptr(new DataTable({std::move(c_fi_names).to_column(), c_fi_values},
                              {"feature_name, feature_importance"})
                             );
  reset_fi();
}


template <typename T>
void Ftrl<T>::init_fi() {
  if (dt_fi == nullptr) return;
  fi = static_cast<T*>(dt_fi->columns[0]->data_w());
}

template <typename T>
void Ftrl<T>::reset_fi() {
  init_fi();
  if (fi == nullptr) return;
  std::memset(fi, 0, nfeatures * sizeof(T));
}


template <typename T>
void Ftrl<T>::define_features(size_t ncols_in) {
  ncols = ncols_in;
  size_t n_inter_features = (interactions)? ncols * (ncols - 1) / 2 : 0;
  nfeatures = ncols + n_inter_features;
}


template <typename T>
void Ftrl<T>::create_hashers(const DataTable* dt) {
  hashers.clear();
  hashers.reserve(ncols);

  for (size_t i = 0; i < ncols; ++i) {
    Column* col = dt->columns[i];
    hashers.push_back(create_colhasher(col));
  }

  // Also, pre-hash column names.
  // TODO: if we stick to default column names, like `C*`,
  // this may only be necessary, when number of features increases.
  const std::vector<std::string>& c_names = dt->get_names();
  colnames_hashes.clear();
  colnames_hashes.reserve(ncols);
  for (size_t i = 0; i < ncols; i++) {
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
  for (size_t i = 0; i < ncols; ++i) {
    // Hash a value adding a column name hash to it, so that the same value
    // in different columns results in different hashes.
    x[i] = (hashers[i]->hash(row) + colnames_hashes[i]) % nbins;
  }

  // Do feature interactions if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach here.
  if (interactions) {
    size_t count = 0;
    for (size_t i = 0; i < ncols - 1; ++i) {
      for (size_t j = i + 1; j < ncols; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_murmur2(s.c_str(), s.length() * sizeof(char), 0);
        x[ncols + count] = h % nbins;
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
*  Get a shallow copy of a feature importance datatable if available.
*/
template <typename T>
DataTable* Ftrl<T>::get_fi() {
  if (dt_fi != nullptr) {
    return dt_fi->copy();
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
size_t Ftrl<T>::get_ncols() {
  return ncols;
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
  init_weights();
  ncols = 0;
  nfeatures = 0;
  model_trained = true;
}


template <typename T>
void Ftrl<T>::set_fi(DataTable* dt_fi_in) {
  dt_fi = dtptr(dt_fi_in->copy());
  nfeatures = dt_fi->nrows;
  init_fi();
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

} // namespace dt

#endif
