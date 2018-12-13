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
#include "frame/py_frame.h"
#include "utils/parallel.h"
#include "extras/dt_ftrl.h"
#include "extras/murmurhash.h"

namespace dt {

/*
*  Set column names for `dt_model` and default parameter values.
*/
const std::vector<std::string> Ftrl::model_cols = {"z", "n"};
const FtrlParams Ftrl::params_default = {0.005, 1.0, 0.0, 1.0,
                                         1000000, 1, false};

/*
*  Set up FTRL parameters and initialize weights.
*/
Ftrl::Ftrl(FtrlParams params_in) :
  dt_model(nullptr),
  z(nullptr),
  n(nullptr),
  params(params_in),
  n_features(0),
  n_inter_features(0),
  model_trained(false)
{
}


void Ftrl::create_model() {
  w = doubleptr(new double[params.d]());
  Column* col_z = Column::new_data_column(SType::FLOAT64, params.d);
  Column* col_n = Column::new_data_column(SType::FLOAT64, params.d);
  dt_model = dtptr(new DataTable({col_z, col_n}, model_cols));
  init_zn();
  reset_model();
}


void Ftrl::reset_model() {
  std::memset(z, 0, params.d * sizeof(double));
  std::memset(n, 0, params.d * sizeof(double));
  model_trained = false;
}


void Ftrl::init_zn() {
  z = static_cast<double*>(dt_model->columns[0]->data_w());
  n = static_cast<double*>(dt_model->columns[1]->data_w());
}


/*
*  Train FTRL model on a dataset.
*/
void Ftrl::fit(const DataTable* dt_X, const DataTable* dt_y) {
  if (dt_model == nullptr || dt_model->nrows != params.d) {
    create_model();
  }

  // Define number of features assuming that the target column is the last one.
  n_features = dt_X->ncols;
  // Calculate number of feature interactions.
  n_inter_features = (params.inter)? n_features * (n_features - 1) / 2 : 0;
  // Create column hashers.
  create_hashers(dt_X);

  // Get the target column.
  auto c_y = static_cast<BoolColumn*>(dt_y->columns[0]);
  auto d_y = c_y->elements_r();

  // Do training for `n_epochs`.
  for (size_t i = 0; i < params.n_epochs; ++i) {
    double total_loss = 0;
    int32_t nth = config::nthreads;

    #pragma omp parallel num_threads(nth)
    {
      // Array to store hashed features and feature interactions.
      uint64ptr x = uint64ptr(new uint64_t[n_features + n_inter_features]);
      int32_t ith = omp_get_thread_num();
      nth = omp_get_num_threads();

      for (size_t j = static_cast<size_t>(ith);
           j < dt_X->nrows;
           j+= static_cast<size_t>(nth)) {

        bool y = d_y[j];
        hash_row(x, j);
        double p = predict_row(x);
        update(x, p, y);

        double loss = logloss(p, y);
        #pragma omp atomic update
        total_loss += loss;
        if ((j+1) % REPORT_FREQUENCY == 0) {
          printf("Training epoch: %zu\tRow: %zu\tPrediction: %f\t"
                 "Current loss: %f\tAverage loss: %f\n",
                 i, j+1, p, loss, total_loss / (j+1));
        }
      }
    }
  }
  model_trained = true;
}


/*
*  Make predictions for a test dataset and return targets as a new datatable.
*  We assume that all the validation is done in `py_ftrl.cc`.
*/
dtptr Ftrl::predict(const DataTable* dt_X) {
  xassert(model_trained);
  // Define number of features
  n_features = dt_X->ncols;
  // Define number of feature interactions.
  n_inter_features = (params.inter)? n_features * (n_features - 1) / 2 : 0;

  // Re-create hashers as stypes for training dataset and predictions
  // may be different
  create_hashers(dt_X);

  // Create a target datatable.
  dtptr dt_y = nullptr;
  Column* col_target = Column::new_data_column(SType::FLOAT64, dt_X->nrows);
  dt_y = dtptr(new DataTable({col_target}, {"target"}));
  auto d_y = static_cast<double*>(dt_y->columns[0]->data_w());

  int32_t nth = config::nthreads;
  #pragma omp parallel num_threads(nth)
  {
    uint64ptr x = uint64ptr(new uint64_t[n_features + n_inter_features]);
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();

    for (size_t j = static_cast<size_t>(ith);
         j < dt_X->nrows;
         j+= static_cast<size_t>(nth)) {

      hash_row(x, j);
      d_y[j] = predict_row(x);
      if ((j+1) % REPORT_FREQUENCY == 0) {
        printf("Row: %zu\tPrediction: %f\n", j+1, d_y[j]);
      }
    }
  }
  return dt_y;
}


/*
*  Make a prediction for an array of hashed features.
*/
double Ftrl::predict_row(const uint64ptr& x) {
  double wTx = 0;
  for (size_t i = 0; i < n_features + n_inter_features; ++i) {
    size_t j = x[i];
    if (fabs(z[j]) <= params.lambda1) {
      w[j] = 0;
    } else {
      w[j] = (signum(z[j]) * params.lambda1 - z[j]) /
             ((params.beta + sqrt(n[j])) / params.alpha + params.lambda2);
    }
    wTx += w[j];
  }
  return sigmoid(wTx);
}


bool Ftrl::is_trained() {
  return model_trained;
}


static hashptr create_colhasher(const Column* col) {
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
    default:             throw ValueError() << "Cannot hash column of type "
                                            << stype;
  }
}


void Ftrl::create_hashers(const DataTable* dt) {
  hashers.clear();
  hashers.reserve(n_features);

  for (size_t i = 0; i < n_features; ++i) {
    Column* col = dt->columns[i];
    hashers.push_back(create_colhasher(col));
  }

  // Also, pre-hash column names.
  // TODO: if we stick to default column names, like `C*`,
  // this may only be necessary, when number of features increases.
  const std::vector<std::string>& c_names = dt->get_names();
  colnames_hashes.clear();
  colnames_hashes.reserve(n_features);
  for (size_t i = 0; i < n_features; i++) {
    uint64_t h = hash_murmur2(c_names[i].c_str(),
                             c_names[i].length() * sizeof(char),
                             0);
    colnames_hashes.push_back(h);
  }
}


/*
*  Sigmoid function.
*/
inline double Ftrl::sigmoid(double x) {
  return 1.0 / (1.0 + exp(-x));
}


/*
*  Bounded sigmoid function.
*/
inline double Ftrl::bsigmoid(double x, double b) {
  double res = 1 / (1 + exp(-std::max(std::min(x, b), -b)));
  return res;
}


/*
*  Update weights based on prediction `p` and the actual target `y`.
*/
void Ftrl::update(const uint64ptr& x, double p, bool y) {
  double g = p - y;

  for (size_t i = 0; i < n_features + n_inter_features; ++i) {
    size_t j = x[i];
    double sigma = (sqrt(n[j] + g * g) - sqrt(n[j])) / params.alpha;
    z[j] += g - sigma * w[j];
    n[j] += g * g;
  }
}


/*
*  Hash each element of the datatable row, do feature interaction if requested.
*/
void Ftrl::hash_row(uint64ptr& x, size_t row) {
  for (size_t i = 0; i < n_features; ++i) {
    // Hash a value adding a column name hash to it, so that the same value
    // in different columns results in different hashes.
    x[i] = (hashers[i]->hash(row) + colnames_hashes[i]) % params.d;
  }

  // Do feature interaction if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach.
  size_t count = 0;
  if (params.inter) {
    for (size_t i = 0; i < n_features - 1; ++i) {
      for (size_t j = i + 1; j < n_features; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_murmur2(s.c_str(), s.length() * sizeof(char), 0);
        x[n_features + count] = h % params.d;
        count++;
      }
    }
  }
}


/*
*  Calculate logloss based on a prediction `p` and the actual target `y`.
*/
double Ftrl::logloss(double p, bool y) {
  double epsilon = std::numeric_limits<double>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  if (y) {
    return -log(p);
  } else {
    return -log(1 - p);
  }
}


/*
*  Calculate signum.
*/
inline double Ftrl::signum(double x) {
  if (x > 0) return 1;
  if (x < 0) return -1;
  return 0;
}


/*
*  Get a shallow copy of an FTRL model if available.
*/
DataTable* Ftrl::get_model() {
  if (dt_model != nullptr) {
    return dt_model->copy();
  } else {
    return nullptr;
  }
}


/*
*  Other getters and setters.
*  Here we assume that all the validation for setters is done in `py_ftrl.cc`.
*/
std::vector<uint64_t> Ftrl::get_colnames_hashes() {
  return colnames_hashes;
}

size_t Ftrl::get_n_features() {
  return n_features;
}


double Ftrl::get_alpha() {
  return params.alpha;
}


double Ftrl::get_beta() {
  return params.beta;
}


double Ftrl::get_lambda1() {
  return params.lambda1;
}


double Ftrl::get_lambda2() {
  return params.lambda2;
}


uint64_t Ftrl::get_d() {
  return params.d;
}


bool Ftrl::get_inter() {
  return params.inter;
}


size_t Ftrl::get_n_epochs() {
  return params.n_epochs;
}


/*
*  Set an FTRL model, assuming all the validation is done in `py_ftrl.cc`
*/
void Ftrl::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  set_d(dt_model->nrows);
  init_zn();
  n_features = 0;
  model_trained = true;
}


void Ftrl::set_alpha(double alpha_in) {
  params.alpha = alpha_in;
}


void Ftrl::set_beta(double beta_in) {
  params.beta = beta_in;
}


void Ftrl::set_lambda1(double lambda1_in) {
  params.lambda1 = lambda1_in;
}


void Ftrl::set_lambda2(double lambda2_in) {
  params.lambda2 = lambda2_in;
}


void Ftrl::set_d(uint64_t d_in) {
  params.d = d_in;
}


void Ftrl::set_inter(bool inter_in) {
  params.inter = inter_in;
}


void Ftrl::set_n_epochs(size_t n_epochs_in) {
  params.n_epochs = n_epochs_in;
}

} // namespace dt

