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
#ifndef dt_EXTRAS_FTRL_h
#define dt_EXTRAS_FTRL_h
#include "py_datatable.h"
#include "utils/parallel.h"
#include "extras/hash.h"


using hashptr = std::unique_ptr<Hash>;
using doubleptr = std::unique_ptr<double[]>;
using uint64ptr = std::unique_ptr<uint64_t[]>;

namespace dt {

struct FtrlParams {
    double alpha;
    double beta;
    double lambda1;
    double lambda2;
    uint64_t d;
    size_t nepochs;
    bool inter;
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
    template <class T1, typename T2>
    void fit(const DataTable*, const T1*, double (*f)(double));
    dtptr predict(const DataTable*, double (*f)(double));
    double predict_row(const uint64ptr&, double (*f)(double));
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
    uint64_t get_d();
    size_t get_nepochs();
    bool get_inter();
    FtrlParams get_params();

    // Setters
    void set_model(DataTable*);
    void set_fi(DataTable*);
    void set_alpha(double);
    void set_beta(double);
    void set_lambda1(double);
    void set_lambda2(double);
    void set_d(uint64_t);
    void set_nepochs(size_t);
    void set_inter(bool);
};


/*
*  Train FTRL model on a dataset.
*/
template <class T1, typename T2>
void Ftrl::fit(const DataTable* dt_X, const T1* c_y, double (*f)(double)) {
  define_features(dt_X->ncols);

  is_dt_valid(dt_model, params.d, 2)? init_weights() : create_model();
  is_dt_valid(dt_fi, nfeatures, 1)? init_fi() : create_fi();

  // Create column hashers.
  create_hashers(dt_X);

  // Get the target column.
  auto d_y = c_y->elements_r();
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
          if (j != RowIndex::NA && !ISNA<T2>(d_y[j])) {
            hash_row(x, i);
            double p = predict_row(x, f);
            double y = static_cast<double>(d_y[j]);
            update(x, p, y);
          }
      }
    }
  }
  model_trained = true;
}

} // namespace dt

#endif
