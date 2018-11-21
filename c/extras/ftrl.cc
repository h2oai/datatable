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
#include "extras/ftrl.h"
#include "extras/murmurhash.h"
#include "frame/py_frame.h"
#include "utils/parallel.h"
#include "datatablemodule.h"


/*
*  Set column names for `dt_model`.
*/
const std::vector<std::string> Ftrl::model_cols = {"z", "n"};
const FtrlParams Ftrl::fp_default = {0.005, 1.0, 0.0, 1.0, 1000000, 1, 1, 0, false};

/*
*  Set up FTRL parameters and initialize weights.
*/
Ftrl::Ftrl(FtrlParams fp_in)
{
  // Set model parameters
  fp = fp_in;
  // Create and initialize model datatable and weight vector.
  create_model();
  init_model();
}


void Ftrl::init_model() {
  model_trained = false;
  z = static_cast<double*>(dt_model->columns[0]->data_w());
  n = static_cast<double*>(dt_model->columns[1]->data_w());
  std::memset(z, 0, fp.d * sizeof(double));
  std::memset(n, 0, fp.d * sizeof(double));
}


void Ftrl::create_model() {
  w = DoublePtr(new double[fp.d]());

  Column* col_z = Column::new_data_column(SType::FLOAT64, fp.d);
  Column* col_n = Column::new_data_column(SType::FLOAT64, fp.d);
  dt_model = dtptr(new DataTable({col_z, col_n}, model_cols));
}


bool Ftrl::is_trained() {
  return model_trained;
}


/*
*  Train FTRL model on a training dataset.
*/
void Ftrl::fit(const DataTable* dt) {
  // Define number of features assuming that the target column is the last one.
  n_features = dt->ncols - 1;

  // Define number of feature interactions.
  n_inter_features = (fp.inter)? n_features * (n_features - 1) / 2 : 0;

  // Get the target column.
  auto c_target = static_cast<BoolColumn*>(dt->columns[dt->ncols - 1]);
  auto d_target = c_target->elements_r();

  // Do training for `n_epochs`.
  for (size_t i = 0; i < fp.n_epochs; ++i) {
    double total_loss = 0;
    int32_t nth = config::nthreads;

    #pragma omp parallel num_threads(nth)
    {
      // Array to store hashed features and feature interactions.
      Uint64Ptr x = Uint64Ptr(new uint64_t[n_features + n_inter_features]);
      int32_t ith = omp_get_thread_num();
      nth = omp_get_num_threads();

      for (size_t j = static_cast<size_t>(ith);
           j < dt->nrows;
           j+= static_cast<size_t>(nth)) {

        bool target = d_target[j];
        hash_row(x, dt, j);
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
  // Create a target datatable.
  dtptr dt_target = nullptr;
  Column* col_target = Column::new_data_column(SType::FLOAT64, dt->nrows);
  dt_target = dtptr(new DataTable({col_target}, {"target"}));
  auto d_target = static_cast<double*>(dt_target->columns[0]->data_w());

  int32_t nth = config::nthreads;

  #pragma omp parallel num_threads(nth)
  {
    Uint64Ptr x = Uint64Ptr(new uint64_t[n_features + n_inter_features]);
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();

    for (size_t j = static_cast<size_t>(ith);
         j < dt->nrows;
         j+= static_cast<size_t>(nth)) {

      hash_row(x, dt, j);
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
double Ftrl::predict_row(const Uint64Ptr& x, size_t x_size) {
  double wTx = 0;
  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    if (fabs(z[i]) <= fp.l1) {
      w[i] = 0;
    } else {
      w[i] = (signum(z[i]) * fp.l1 - z[i]) /
             ((fp.b + sqrt(n[i])) / fp.a + fp.l2);
    }
    wTx += w[i];
  }

  return sigmoid(wTx);
}


/*
*  Sigmoid function.
*/
inline double Ftrl::sigmoid(double x) {
  double res = 1.0 / (1.0 + exp(-x));

  return res;
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
void Ftrl::update(const Uint64Ptr& x, size_t x_size, double p, bool target) {
  double g = p - target;

  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    double sigma = (sqrt(n[i] + g * g) - sqrt(n[i])) / fp.a;
    z[i] += g - sigma * w[i];
    n[i] += g * g;
  }
}


/*
*  Hash string using the specified hash function. This is for the tests only,
*  for production we will remove this method and stick to the hash function,
*  that demonstrates the best performance.
*/
uint64_t Ftrl::hash_string(const char * key, size_t len) {
  uint64_t res;
  switch (fp.hash_type) {
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
    case 1:  res = hash_murmur2(key, len, fp.seed); break;

    // 128 bits Murmur3 hash function, similar performance to `hash_murmur2`.
    case 2:  {
                uint64_t h[2];
                hash_murmur3(key, len, fp.seed, h);
                res = h[0];
                break;
             }
    default: res = hash_murmur2(key, len, fp.seed);
  }
  return res;
}


/*
*  Hash each element of the datatable row, do feature interaction is requested.
*/
void Ftrl::hash_row(Uint64Ptr& x, const DataTable* dt, size_t row_id) {
  std::vector<std::string> c_names = dt->get_names();
  uint64_t index;

  for (size_t i = 0; i < n_features; ++i) {
    std::string str;
    Column* c = dt->columns[i];
    LType ltype = info(c->stype()).ltype();

    switch (ltype) {
      // For booleans, just cast `bool` to `uint64_t`.
      case LType::BOOL:    {
                             auto c_bool = static_cast<BoolColumn*>(c);
                             auto d_bool = c_bool->elements_r();
                             index = static_cast<uint64_t>(d_bool[row_id]);
                             break;
                           }

      // For integers, just cast `int32_t` to `uint64_t`. This also works fine
      // for negative numbers as they're casted to very large `uint64_t`.
      case LType::INT:     {
                             auto c_int = static_cast<IntColumn<int32_t>*>(c);
                             auto d_int = c_int->elements_r();
                             index = static_cast<uint64_t>(d_int[row_id]);
                             break;
                           }

      // For doubles, use their bit representation as `uint64_t`.
      case LType::REAL:    {
                             auto c_real = static_cast<RealColumn<double>*>(c);
                             auto d_real = c_real->elements_r();
                             index = hash_double(d_real[row_id]);
                             break;
                           }

      // For strings, invoke `hash_string()` that will call the actual
      // hash function based on the provided `hash_type`.
      case LType::STRING:  {
                             auto c_string = static_cast<StringColumn<uint32_t>*>(c);
                             auto d_string = c_string->offsets();
                             const char* strdata = c_string->strdata();
                             const uint32_t strstart = d_string[row_id-1] & ~GETNA<uint32_t>();
                             const char* c_str = strdata + strstart;
                             uint32_t len = d_string[row_id] - strstart;
                             index = hash_string(c_str, len * sizeof(char));
                             break;
                           }
      default:             throw ValueError() << "Datatype is not supported!";
    }
    // Add the column name hash to the hashed value, so that the same value
    // in different columns will result in different hashes.
    // TODO: pre-hash all the column names only once.
    uint64_t h = hash_string(c_names[i].c_str(), c_names[i].length() * sizeof(char));
    index += h;
    x[i] = index % fp.d;
  }

  // Do feature interaction if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach.
  size_t count = 0;
  if (fp.inter) {
    for (size_t i = 0; i < n_features - 1; ++i) {
      for (size_t j = i + 1; j < n_features; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_string(s.c_str(), s.length() * sizeof(char));
        x[n_features + count] = h % fp.d;
        count++;
      }
    }
  }
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
*  Hash `double` to `uint64_t` based on its bit representation.
*/
inline uint64_t Ftrl::hash_double(double x) {
  uint64_t* h = reinterpret_cast<uint64_t*>(&x);
  return *h;
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
*  Set an FTRL model, assuming all the validation is done in `py_ftrl.cc`
*/
void Ftrl::set_model(DataTable* dt_model_in) {
  dt_model = dtptr(dt_model_in->copy());
}


/*
*  Other getters and setters.
*  Here we assume that all the validation is done in `py_ftrl.cc`.
*/
double Ftrl::get_a() {
  return fp.a;
}


double Ftrl::get_b() {
  return fp.b;
}


double Ftrl::get_l1() {
  return fp.l1;
}


double Ftrl::get_l2() {
  return fp.l2;
}


uint64_t Ftrl::get_d() {
  return fp.d;
}


bool Ftrl::get_inter() {
  return fp.inter;
}


unsigned int Ftrl::get_hash_type() {
  return fp.hash_type;
}


unsigned int Ftrl::get_seed() {
  return fp.seed;
}


size_t Ftrl::get_n_epochs() {
  return fp.n_epochs;
}


void Ftrl::set_a(double a_in) {
  if (fp.a != a_in) {
    fp.a = a_in;
    init_model();
  }
}


void Ftrl::set_b(double b_in) {
  if (fp.b != b_in) {
    fp.b = b_in;
    init_model();
  }
}


void Ftrl::set_l1(double l1_in) {
  if (fp.l1 != l1_in) {
    fp.l1 = l1_in;
    init_model();
  }
}


void Ftrl::set_l2(double l2_in) {
  if (fp.l2 != l2_in) {
    fp.l2 = l2_in;
    init_model();
  }
}


void Ftrl::set_d(uint64_t d_in) {
  if (fp.d != d_in) {
    fp.d = d_in;
    create_model();
    init_model();
  }
}


void Ftrl::set_inter(bool inter_in) {
  if (fp.inter != inter_in) {
    fp.inter = inter_in;
    init_model();
  }
}


void Ftrl::set_hash_type(unsigned int hash_type_in) {
  if (fp.hash_type != hash_type_in) {
    fp.hash_type = hash_type_in;
    init_model();
  }
}


void Ftrl::set_seed(unsigned int seed_in) {
  if (fp.seed != seed_in) {
    fp.seed = seed_in;
    init_model();
  }
}


void Ftrl::set_n_epochs(size_t n_epochs_in) {
  if (fp.n_epochs != n_epochs_in) {
    fp.n_epochs = n_epochs_in;
  }
}
