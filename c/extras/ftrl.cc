//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_EXTRAS_FTRL_cc
#include "extras/ftrl.h"
#include "frame/py_frame.h"
#include "py_utils.h"
#include "python/obj.h"
#include "rowindex.h"
#include "types.h"
#include "utils/parallel.h"
#include "utils/shared_mutex.h"
#include <stdio.h>


/*
*  Reading data from Python, training model and making predictions.
*/
PyObject* ftrl(PyObject*, PyObject* args) {

  double a, b, l1, l2;
  unsigned int d, n_epochs, hash_type;
  bool inter;
  PyObject* arg1;
  PyObject* arg2;

  if (!PyArg_ParseTuple(args, "OOddddIIpI:ftrl", &arg1, &arg2, &a, &b, &l1, &l2,
                        &d, &n_epochs, &inter, &hash_type)) return nullptr;

  DataTable* dt_train = py::obj(arg1).to_frame();
  DataTable* dt_test = py::obj(arg2).to_frame();

  Ftrl ft(a, b, l1, l2, d, n_epochs, inter, hash_type);
  ft.train(dt_train);
  DataTable* dt_target = ft.test(dt_test).release();
  py::Frame* frame_target = py::Frame::from_datatable(dt_target);

  return frame_target;
}


/*
*  Setting up FTRL parameters and initializing weights.
*/
Ftrl::Ftrl(double a_in, double b_in, double l1_in, double l2_in,
           size_t d_in, size_t nepochs_in, bool inter_in, size_t hash_type_in) :
  a(a_in),
  b(b_in),
  l1(l1_in),
  l2(l2_in),
  d(d_in),
  n_epochs(nepochs_in),
  inter(inter_in),
  hash_type(hash_type_in)
{
  n = DoublePtr(new double[d]);
  z = DoublePtr(new double[d]);
  w = DoublePtr(new double[d]);

  std::memset(n.get(), 0, d * sizeof(double));
  std::memset(w.get(), 0, d * sizeof(double));
//  std::memset(z.get(), 0, d * sizeof(double));

  // Initialize weights with random numbers from [0; 1]
  for (size_t i = 0; i < d; ++i){
    z[i] = static_cast<double>(rand()) / RAND_MAX;
  }
}


/*
*  Train FTRL model on training data.
*/
void Ftrl::train(const DataTable* dt) {
  // This includes a bias term, and `dt->ncols - 1` columns, i.e. excluding the target column.
  n_features = size_t(dt->ncols);
  // Number of feature interactions.
  if (inter) {
    n_features_inter = (n_features - 1) * (n_features - 2) / 2;
  } else {
    n_features_inter = 0;
  }

  // Get the target column.
  Column* cy = dt->columns[dt->ncols - 1];
  auto cy_bool = static_cast<BoolColumn*>(cy);
  auto dy_bool = cy_bool->elements_r();

  // Do training for `n_epochs`.
  for (size_t i = 0; i < n_epochs; ++i) {
    double loss = 0;
    int32_t nth = config::nthreads;

    #pragma omp parallel num_threads(nth)
    {
      SizetPtr x = SizetPtr(new size_t[n_features + n_features_inter]);
      x[0] = 0; // Bias term
      int32_t ith = omp_get_thread_num();
      nth = omp_get_num_threads();

      for (int32_t j = ith; j < dt->nrows; j+= nth) {
        bool y = dy_bool[j];
        hash(x, dt, j);
        double p = predict(x, n_features + n_features_inter);
        double ll = logloss(p, y);

        #pragma omp atomic update
        loss += ll;

        if ((j+1) % REPORT_FREQUENCY == 0) {
          printf("Training epoch: %zu\t row: %d\t prediction: %f\t loss: %f\t average loss: %f\n",
                  i, j+1, p, ll, loss / (j+1));
        }
        update(x, n_features + n_features_inter, p, y);
      }
    }
  }
}


/*
*  Make predictions on test data and return targets as a new datatable.
*/
DataTablePtr Ftrl::test(const DataTable* dt) {
  // Create a target datatable.
  DataTablePtr dt_target = nullptr;
  Column** cols_target = dt::amalloc<Column*>(static_cast<int64_t>(2));
  cols_target[0] = Column::new_data_column(SType::FLOAT64, dt->nrows);
  cols_target[1] = nullptr;
  dt_target = DataTablePtr(new DataTable(cols_target, {"target"}));
  auto d_target = static_cast<double*>(dt_target->columns[0]->data_w());

  int32_t nth = config::nthreads;

  #pragma omp parallel num_threads(nth)
  {
    SizetPtr x = SizetPtr(new size_t[n_features + n_features_inter]);
    x[0] = 0; // Bias term
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();

    for (int32_t j = ith; j < dt->nrows; j+= nth) {
      hash(x, dt, j);
      d_target[j] = predict(x, n_features + n_features_inter);
      if ((j+1) % REPORT_FREQUENCY == 0) {
        printf("Testing row: %d\t prediction: %f\n", j+1, d_target[j]);
      }
    }

  }
  return dt_target;
}


/*
*  Make predictions for a hashed row.
*/
double Ftrl::predict(const SizetPtr& x, size_t x_size) {
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

  return sigmoid(-wTx);
}


/*
*  Sigmoid function.
*/
double Ftrl::sigmoid(double x) {
  double res = 1.0 / (1.0 + exp(-x));

  return res;
}


/*
*  Bounded sigmoid function.
*/
double Ftrl::bsigmoid(double x, double b) {
  double res = 1 / (1 + exp(-std::max(std::min(x, b), -b)));

  return res;
}


/*
*  Update weights based on prediction and the actual value.
*/
void Ftrl::update(const SizetPtr& x, size_t x_size, double p, bool y) {
  double g = p - y;

  for (size_t j = 0; j < x_size; ++j) {
    size_t i = x[j];
    double sigma = (sqrt(n[i] + g * g) - sqrt(n[i])) / a;
    z[i] += g - sigma * w[i];
    n[i] += g * g;
  }
}


/*
*  Choose a hashing method.
*/
void Ftrl::hash(SizetPtr& x, const DataTable* dt, int64_t row_id) {
  if (hash_type) {
    hash_string(x, dt, row_id);
  } else {
    hash_numeric(x, dt, row_id);
  }
}


/*
*  Do std::hashing leaving numeric values as they are.
*  May not work well, here just for testing purposes.
*/
void Ftrl::hash_numeric(SizetPtr& x, const DataTable* dt, int64_t row_id) {
  std::vector<std::string> c_names = dt->get_names();

  for (size_t i = 0; i < static_cast<size_t>(dt->ncols) - 1; ++i) {
    size_t index;
    Column* c = dt->columns[i];
    LType ltype = info(c->stype()).ltype();

    switch (ltype) {
      case LType::BOOL:    {
                             auto c_bool = static_cast<BoolColumn*>(c);
                             auto d_bool = c_bool->elements_r();
                             index = static_cast<size_t>(d_bool[row_id]);
                             break;
                           }
      case LType::INT:     {
                             auto c_int = static_cast<IntColumn<int32_t>*>(c);
                             auto d_int = c_int->elements_r();
                             index = static_cast<size_t>(d_int[row_id]);
                             break;
                           }
      case LType::REAL:    {
                             auto c_real = static_cast<RealColumn<double>*>(c);
                             auto d_real = c_real->elements_r();
                             index = static_cast<size_t>(d_real[row_id]);
                             break;
                           }
      case LType::STRING:  {
                             auto c_string = static_cast<StringColumn<uint32_t>*>(c);
                             auto d_string = c_string->offsets();
                             const char* strdata = c_string->strdata();
                             const char* c_str;
                             c_str = strdata + (d_string[row_id - 1] & ~GETNA<uint32_t>());
                             uint32_t l = d_string[row_id] - (d_string[row_id - 1] & ~GETNA<uint32_t>());
                             std::string str(c_str, c_str + l);
                             index = std::hash<std::string>{}(str);
                             break;
                           }
      default:             throw ValueError() << "Datatype is not supported";
    }
    x[i+1] = index % d;
  }

  if (inter) {
    // TODO: `inter` order feature interaction.
    // Make sure `x` has enough memory allocated for this purpose.
  }
}


/*
*  Do std::hashing for `col_name` + `_` + `col_value`, casting all the
*  `col_value`s to `string`. Needs optimization in terms of performance.
*/
void Ftrl::hash_string(SizetPtr& x, const DataTable* dt, int64_t row_id) {
  std::vector<std::string> c_names = dt->get_names();
  size_t index;

  for (size_t i = 0; i < n_features - 1; ++i) {
    std::string str;
    Column* c = dt->columns[i];

    LType ltype = info(c->stype()).ltype();
    switch (ltype) {
      case LType::BOOL:    {
                             auto c_bool = static_cast<BoolColumn*>(c);
                             auto d_bool = c_bool->elements_r();
                             str = std::to_string(d_bool[row_id]);
                             break;
                           }
      case LType::INT:     {
                             auto c_int = static_cast<IntColumn<int32_t>*>(c);
                             auto d_int = c_int->elements_r();
                             str = std::to_string(d_int[row_id]);
                             break;
                           }
      case LType::REAL:    {
                             auto c_real = static_cast<RealColumn<double>*>(c);
                             auto d_real = c_real->elements_r();
                             str = std::to_string(d_real[row_id]);
                             break;
                           }
      case LType::STRING:  {
                             auto c_string = static_cast<StringColumn<uint32_t>*>(c);
                             auto d_string = c_string->offsets();
                             const char* strdata = c_string->strdata();
                             const char* c_str = strdata + (d_string[row_id - 1] & ~GETNA<uint32_t>());
                             uint32_t l = d_string[row_id] - (d_string[row_id - 1] & ~GETNA<uint32_t>());
                             str.assign(c_str, c_str + l);
                             break;
                           }
      default:             throw ValueError() << "Datatype is not supported";
    }
    index = std::hash<std::string>{}(c_names[i] + '_' + str);
    x[i + 1] = index % d;
  }

  size_t count = 0;
  if (inter) {
    for (size_t i = 0; i < n_features - 1; ++i) {
      for (size_t j = i + 1; j < n_features - 1; ++j) {
        index = std::hash<std::string>{}(std::to_string(x[i+1]) + '_' + std::to_string(x[j+1]));
        x[n_features + count] = index % d;
        count++;
      }
    }
  }
}


/*
*  Calculate logloss based on a prediction and the actual value.
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
double Ftrl::signum(double x) {
  if (x > 0) return 1;
  if (x < 0) return -1;
  return 0;
}
