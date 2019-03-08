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
#include "models/hash.h"
#include "utils/parallel.h"
#include "options.h"


using hashptr = std::unique_ptr<Hash>;
using doubleptr = std::unique_ptr<double[]>;
using uint64ptr = std::unique_ptr<uint64_t[]>;

namespace dt {

struct FtrlParams {
    double alpha;
    double beta;
    double lambda1;
    double lambda2;
    uint64_t nbins;
    size_t nepochs;
    bool interactions;
    size_t : 56;
};


class Ftrl {
  private:
    // Model datatable, column pointers and weight vector
    dtptr dt_model;
    double* z;
    double* n;
    doubleptr w;

    // Feature importance datatable and column pointer
    dtptr dt_fi;
    double* fi;

    // Input parameters
    FtrlParams params;

    // Number of columns in a training datatable and total number of features
    size_t ncols;
    size_t nfeatures;

    // Set this to `True` in `train` and `set_model` methods
    bool model_trained;
    size_t : 56;

    // Hashers and hashed column names
    std::vector<hashptr> hashers;
    std::vector<uint64_t> colnames_hashes;

  public:
    Ftrl(FtrlParams);

    static const std::vector<std::string> model_colnames;
    static const FtrlParams default_params;

    // Learning and predicting methods.
    template <typename T, typename F>
    void fit(const DataTable*, const Column*, F);
    template<typename F> dtptr predict(const DataTable*, F f);
    double predict_row(const uint64ptr&);
    void update(const uint64ptr&, double, double);

    // Model and feature importance handling methods
    void create_model();
    void reset_model();
    void init_weights();
    void create_fi();
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

    // Learning helper methods
    static double logloss(double, bool);

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
*  Train FTRL model on a dataset.
*/
template <typename T, typename F>
void Ftrl::fit(const DataTable* dt_X, const Column* c_y, F f) {
  define_features(dt_X->ncols);

  is_dt_valid(dt_model, params.nbins, 2)? init_weights() : create_model();
  is_dt_valid(dt_fi, nfeatures, 1)? init_fi() : create_fi();

  // Create column hashers.
  create_hashers(dt_X);

  // Get the target column.
  auto d_y = static_cast<const T*>(c_y->data());
  const RowIndex ri_y = c_y->rowindex();

  // Do training for `nepochs`.
  for (size_t e = 0; e < params.nepochs; ++e) {
    #pragma omp parallel num_threads(config::nthreads)
    {
      // Array to store hashed column values and their interactions.
      uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());

      for (size_t i = ith; i < dt_X->nrows; i += nth) {
          size_t j = ri_y[i];
          if (j != RowIndex::NA && !ISNA<T>(d_y[j])) {
            hash_row(x, i);
            double p = f(predict_row(x));
            double y = static_cast<double>(d_y[j]);
            update(x, p, y);
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
template<typename F>
dtptr Ftrl::predict(const DataTable* dt_X, F f) {
  xassert(model_trained);
  define_features(dt_X->ncols);
  init_weights();
  is_dt_valid(dt_fi, nfeatures, 1)? init_fi() : create_fi();

  // Re-create hashers as stypes for training dataset and predictions
  // may be different
  create_hashers(dt_X);

  // Create a target datatable.
  dtptr dt_y;
  Column* col_target = Column::new_data_column(SType::FLOAT64, dt_X->nrows);
  dt_y = dtptr(new DataTable({col_target}, {"target"}));
  auto d_y = static_cast<double*>(dt_y->columns[0]->data_w());

  #pragma omp parallel num_threads(config::nthreads)
  {
    uint64ptr x = uint64ptr(new uint64_t[nfeatures]);
    size_t ith = static_cast<size_t>(omp_get_thread_num());
    size_t nth = static_cast<size_t>(omp_get_num_threads());

    for (size_t i = ith; i < dt_X->nrows; i += nth) {
      hash_row(x, i);
      d_y[i] = f(predict_row(x));
    }
  }
  return dt_y;
}


} // namespace dt

#endif
