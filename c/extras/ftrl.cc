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
#include "extras/murmurhash.h"
#include "frame/py_frame.h"
#include "utils/parallel.h"
#include "extras/ftrl.h"


/*
*  Set column names for `dt_model` and default parameter values.
*/
const std::vector<std::string> Ftrl::model_cols = {"z", "n"};
const FtrlParams Ftrl::params_default = {0.005, 1.0, 0.0, 1.0,
                                         1000000, 1, 1, 0, false};

/*
*  Set up FTRL parameters and initialize weights.
*/
Ftrl::Ftrl(FtrlParams params_in)
{
  // Set model parameters
  params = params_in;
  // Create and initialize model datatable and weight vector.
  create_model();
  init_model();
}


void Ftrl::init_model() {
  n_features = 0;
  n_inter_features = 0;
  model_trained = false;
  z = static_cast<double*>(dt_model->columns[0]->data_w());
  n = static_cast<double*>(dt_model->columns[1]->data_w());
  std::memset(z, 0, params.d * sizeof(double));
  std::memset(n, 0, params.d * sizeof(double));
}


void Ftrl::create_model() {
  w = doubleptr(new double[params.d]());

  Column* col_z = Column::new_data_column(SType::FLOAT64, params.d);
  Column* col_n = Column::new_data_column(SType::FLOAT64, params.d);
  dt_model = dtptr(new DataTable({col_z, col_n}, model_cols));
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
    default:             throw ValueError() << "Cannot hash column of type " << stype;
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
*  Train FTRL model on a dataset.
*/
void Ftrl::fit(const DataTable* dt) {
  // Define number of features assuming that the target column is the last one.
  n_features = dt->ncols - 1;
  create_hashers(dt);

  // Define number of feature interactions.
  n_inter_features = (params.inter)? n_features * (n_features - 1) / 2 : 0;

  // Get the target column.
  auto c_target = static_cast<BoolColumn*>(dt->columns[dt->ncols - 1]);
  auto d_target = c_target->elements_r();

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
           j < dt->nrows;
           j+= static_cast<size_t>(nth)) {

        bool target = d_target[j];
        hash_row(x, j);
        double p = predict_row(x, n_features + n_inter_features);
        update(x, n_features + n_inter_features, p, target);

        double loss = logloss(p, target);
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
dtptr Ftrl::predict(const DataTable* dt) {
  create_hashers(dt);

  // Create a target datatable.
  dtptr dt_target = nullptr;
  Column* col_target = Column::new_data_column(SType::FLOAT64, dt->nrows);
  dt_target = dtptr(new DataTable({col_target}, {"target"}));
  auto d_target = static_cast<double*>(dt_target->columns[0]->data_w());

  int32_t nth = config::nthreads;

  #pragma omp parallel num_threads(nth)
  {
    uint64ptr x = uint64ptr(new uint64_t[n_features + n_inter_features]);
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();

    for (size_t j = static_cast<size_t>(ith);
         j < dt->nrows;
         j+= static_cast<size_t>(nth)) {

      hash_row(x, j);
      d_target[j] = predict_row(x, n_features + n_inter_features);
      if ((j+1) % REPORT_FREQUENCY == 0) {
        printf("Row: %zu\tPrediction: %f\n", j+1, d_target[j]);
      }
    }
  }
  return dt_target;
}


/*
*  Make a prediction for an array of hashed features.
*/
double Ftrl::predict_row(const uint64ptr& x, size_t x_size) {
  double wTx = 0;
  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    if (fabs(z[i]) <= params.lambda1) {
      w[i] = 0;
    } else {
      w[i] = (signum(z[i]) * params.lambda1 - z[i]) /
             ((params.beta + sqrt(n[i])) / params.alpha + params.lambda2);
    }
    wTx += w[i];
  }

  return sigmoid(wTx);
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
*  Update weights based on prediction and the actual target.
*/
void Ftrl::update(const uint64ptr& x, size_t x_size, double p, bool target) {
  double g = p - target;

  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    double sigma = (sqrt(n[i] + g * g) - sqrt(n[i])) / params.alpha;
    z[i] += g - sigma * w[i];
    n[i] += g * g;
  }
}


/*
*  Hash each element of the datatable row, do feature interaction is requested.
*/
void Ftrl::hash_row(uint64ptr& x, size_t row_id) {
  for (size_t i = 0; i < n_features; ++i) {
    uint64_t index = hashers[i]->hash(row_id);
    // Add the column name hash to the hashed value, so that the same value
    // in different columns will result in different hashes.
    index += colnames_hashes[i];
    x[i] = index % params.d;
  }

  // Do feature interaction if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach.
  size_t count = 0;
  if (params.inter) {
    for (size_t i = 0; i < n_features - 1; ++i) {
      for (size_t j = i + 1; j < n_features; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_string(s.c_str(), s.length() * sizeof(char));
        x[n_features + count] = h % params.d;
        count++;
      }
    }
  }
}


/*
*  Hash string using the specified hash function. This is for the tests only,
*  for production we will remove this method and stick to the hash function,
*  that demonstrates the best performance.
*/
uint64_t Ftrl::hash_string(const char * key, size_t len) {
  uint64_t res;
  switch (params.hash_type) {
    // `std::hash` is kind of slow, because we need to convert `char*` to
    // `std::string`, as `std::hash<char*>` doesn't hash
    // the actual data.
    case 0:  {
                std::string str(key,
                                key + len / sizeof(char));
                res = std::hash<std::string>{}(str);
                break;
             }
    // 64 bits Murmur2 hash function. The best performer so far,
    // need to test it for the memory alignment issues.
    case 1:  res = hash_murmur2(key, len, params.seed); break;

    // 128 bits Murmur3 hash function, similar performance to `hash_murmur2`.
    case 2:  {
                uint64_t h[2];
                hash_murmur3(key, len, params.seed, h);
                res = h[0];
                break;
             }
    default: res = hash_murmur2(key, len, params.seed);
  }
  return res;
}


/*
*  Calculate logloss based on a prediction and the actual target.
*/
double Ftrl::logloss(double p, bool target) {
  double epsilon = std::numeric_limits<double>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  if (target) {
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


unsigned int Ftrl::get_hash_type() {
  return params.hash_type;
}


unsigned int Ftrl::get_seed() {
  return params.seed;
}


size_t Ftrl::get_n_epochs() {
  return params.n_epochs;
}


/*
*  Set an FTRL model, assuming all the validation is done in `py_ftrl.cc`
*/
void Ftrl::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
  model_trained = true;
}


void Ftrl::set_alpha(double a_in) {
  if (params.alpha != a_in) {
    params.alpha = a_in;
    init_model();
  }
}


void Ftrl::set_beta(double b_in) {
  if (params.beta != b_in) {
    params.beta = b_in;
    init_model();
  }
}


void Ftrl::set_lambda1(double l1_in) {
  if (params.lambda1 != l1_in) {
    params.lambda1 = l1_in;
    init_model();
  }
}


void Ftrl::set_lambda2(double l2_in) {
  if (params.lambda2 != l2_in) {
    params.lambda2 = l2_in;
    init_model();
  }
}


void Ftrl::set_d(uint64_t d_in) {
  if (params.d != d_in) {
    params.d = d_in;
    create_model();
    init_model();
  }
}


void Ftrl::set_inter(bool inter_in) {
  if (params.inter != inter_in) {
    params.inter = inter_in;
    init_model();
  }
}


void Ftrl::set_hash_type(unsigned int hash_type_in) {
  if (params.hash_type != hash_type_in) {
    params.hash_type = hash_type_in;
    init_model();
  }
}


void Ftrl::set_seed(unsigned int seed_in) {
  if (params.seed != seed_in) {
    params.seed = seed_in;
    init_model();
  }
}


void Ftrl::set_n_epochs(size_t n_epochs_in) {
  if (params.n_epochs != n_epochs_in) {
    params.n_epochs = n_epochs_in;
  }
}
