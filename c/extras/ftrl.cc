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
*  Read data from Python, train FTRL model and make predictions.
*/
namespace py {
  static PKArgs ftrl(
    11, 0, 0, false, false,
    {"df_train", "df_test", "a", "b", "l1", "l2", "d", "n_epochs", "inter",
     "hash_type", "seed"}, "ftrl", "",
     [](const py::PKArgs& args) -> py::oobj {
       DataTable* dt_train = args[0].to_frame();
       DataTable* dt_test = args[1].to_frame();

       double a = args[2].to_double();
       double b = args[3].to_double();
       double l1 = args[4].to_double();
       double l2 = args[5].to_double();

       uint64_t d = static_cast<uint64_t>(args[6].to_size_t());
       size_t n_epochs = args[7].to_size_t();
       bool inter = args[8].to_bool_strict();
       unsigned int hash_type = static_cast<unsigned int>(args[9].to_size_t());
       unsigned int seed = static_cast<unsigned int>(args[10].to_size_t());

       Ftrl ft(a, b, l1, l2, d, n_epochs, inter, hash_type, seed);
       ft.train(dt_train);
       DataTable* dt_target = ft.test(dt_test).release();
       py::Frame* frame_target = py::Frame::from_datatable(dt_target);

       return frame_target;
     }
  );
}


/*
*  Set up FTRL parameters and initialize weights.
*/
Ftrl::Ftrl(double a_in, double b_in, double l1_in, double l2_in,
           uint64_t d_in, size_t nepochs_in, bool inter_in,
           unsigned int hash_type_in, unsigned int seed_in) :
  a(a_in),
  b(b_in),
  l1(l1_in),
  l2(l2_in),
  d(d_in),
  n_epochs(nepochs_in),
  hash_type(hash_type_in),
  seed(seed_in),
  inter(inter_in)
{
  n = DoublePtr(new double[d]());
  w = DoublePtr(new double[d]());

  // Initialize weights with random [0; 1] numbers
  z = DoublePtr(new double[d]);
  srand(seed);
  for (uint64_t i = 0; i < d; ++i){
    z[i] = static_cast<double>(rand()) / RAND_MAX;
  }
}


/*
*  Train FTRL model on a training dataset.
*/
void Ftrl::train(const DataTable* dt) {
  // Define number of features that equal to one bias term
  // plus `dt->ncols - 1` columns. We assume that the target column
  // is the last one.
  n_features = dt->ncols;

  // Define number of feature interactions.
  n_inter_features = (inter)? (n_features - 1) * (n_features - 2) / 2 : 0;

  // Get the target column.
  auto c_target = static_cast<BoolColumn*>(dt->columns[dt->ncols - 1]);
  auto d_target = c_target->elements_r();

  // Do training for `n_epochs`.
  for (size_t i = 0; i < n_epochs; ++i) {
    double total_loss = 0;
    int32_t nth = config::nthreads;

    #pragma omp parallel num_threads(nth)
    {
      // Array to store hashed features and feature interactions.
      Uint64Ptr x = Uint64Ptr(new uint64_t[n_features + n_inter_features]);
      // Bias term, do we need it? Results are quite similar with/without bias.
      x[0] = 0;
      int32_t ith = omp_get_thread_num();
      nth = omp_get_num_threads();

      for (size_t j = static_cast<size_t>(ith);
           j < dt->nrows;
           j+= static_cast<size_t>(nth)) {

        bool target = d_target[j];
        hash_row(x, dt, j);
        double p = predict(x, n_features + n_inter_features);
        double loss = logloss(p, target);

        #pragma omp atomic update
        total_loss += loss;

        if ((j+1) % REPORT_FREQUENCY == 0) {
          printf("Training epoch: %zu\tRow: %zu\tPrediction: %f\t"
                 "Current loss: %f\tAverage loss: %f\n",
                 i, j+1, p, loss, total_loss / (j+1));
        }
        update(x, n_features + n_inter_features, p, target);
      }
    }
  }
}


/*
*  Make predictions for a test dataset and return targets as a new datatable.
*/
dtptr Ftrl::test(const DataTable* dt) {
  // Create a datatable to store targets.
  dtptr dt_target = nullptr;
  Column* col_target = Column::new_data_column(SType::FLOAT64, dt->nrows);
  dt_target = dtptr(new DataTable({col_target}, {"target"}));
  auto d_target = static_cast<double*>(dt_target->columns[0]->data_w());

  int32_t nth = config::nthreads;

  #pragma omp parallel num_threads(nth)
  {
    Uint64Ptr x = Uint64Ptr(new uint64_t[n_features + n_inter_features]);
    x[0] = 0;
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();

    for (size_t j = static_cast<size_t>(ith);
         j < dt->nrows;
         j+= static_cast<size_t>(nth)) {

      hash_row(x, dt, j);
      d_target[j] = predict(x, n_features + n_inter_features);
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
double Ftrl::predict(const Uint64Ptr& x, size_t x_size) {
  double wTx = 0;
  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    if (fabs(z[i]) <= l1) {
      w[i] = 0;
    } else {
      w[i] = (signum(z[i]) * l1 - z[i]) / ((b + sqrt(n[i])) / a + l2);
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
    double sigma = (sqrt(n[i] + g * g) - sqrt(n[i])) / a;
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
  switch (hash_type) {
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
    case 1:  res = hash_murmur2(key, len, seed); break;

    // 128 bits Murmur3 hash function, similar performance to `hash_murmur2`.
    case 2:  {
                uint64_t h[2];
                hash_murmur3(key, len, seed, h);
                res = h[0];
                break;
             }
    default: res = hash_murmur2(key, len, seed);
  }
  return res;
}


/*
*  Hash each element of the datatable row, do feature interaction is requested.
*/
void Ftrl::hash_row(Uint64Ptr& x, const DataTable* dt, size_t row_id) {
  std::vector<std::string> c_names = dt->get_names();
  uint64_t index;

  for (size_t i = 0; i < n_features - 1; ++i) {
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
    uint64_t h = hash_string(c_names[i].c_str(), c_names[i].length() * sizeof(char));
    index += h;
    x[i+1] = index % d;
  }

  // Do feature interaction if required. We may also want to test
  // just a simple `h = x[i+1] + x[j+1]` approach.
  size_t count = 0;
  if (inter) {
    for (size_t i = 0; i < n_features - 1; ++i) {
      for (size_t j = i + 1; j < n_features - 1; ++j) {
        std::string s = std::to_string(x[i+1]) + std::to_string(x[j+1]);
        uint64_t h = hash_string(s.c_str(), s.length() * sizeof(char));
        x[n_features + count] = h % d;
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


void DatatableModule::init_methods_ftrl() {
  ADDFN(py::ftrl);
}
