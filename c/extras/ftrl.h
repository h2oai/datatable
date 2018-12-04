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
#include "extras/hash.h"


typedef std::unique_ptr<Hash> hashptr;
typedef std::unique_ptr<double[]> doubleptr;
typedef std::unique_ptr<uint64_t[]> uint64ptr;
#define REPORT_FREQUENCY 1000


struct FtrlParams {
    double alpha;
    double beta;
    double lambda1;
    double lambda2;
    uint64_t d;
    size_t n_epochs;
    unsigned int hash_type;
    unsigned int seed;
    bool inter;
    size_t : 56;
};


class Ftrl {
  private:
    dtptr dt_model;
    double* z;
    double* n;
    
    // Input to the model.
    FtrlParams params;

    // Calculated during the learning process.
    size_t n_features;
    size_t n_inter_features;
    doubleptr w;
    bool model_trained;
    size_t : 56;

    // Hashed column names and column hashers
    std::vector<uint64_t> colnames_hashes;
    std::vector<hashptr> hashers;

  public:
    Ftrl(FtrlParams);

    static const std::vector<std::string> model_cols;
    static const FtrlParams params_default;

    // Learning and predicting methods.
    void fit(const DataTable*);
    dtptr predict(const DataTable*);
    double predict_row(const uint64ptr&, size_t);
    void update(const uint64ptr&, size_t, double, bool);

    bool is_trained();
    void init_model();
    void create_model();

    // Learning helper methods.
    static double logloss(double, bool);
    static double signum(double);
    static double sigmoid(double);
    static double bsigmoid(double, double);

    // Hashing methods.
    void create_hashers(const DataTable*);
    void hash_row(uint64ptr&, size_t);
    uint64_t hash_string(const char *, size_t);

    // Getters and setters, some will invalidate the learning results.
    DataTable* get_model();
    size_t get_n_features();
    std::vector<uint64_t> get_colnames_hashes();
    double get_alpha();
    double get_beta();
    double get_lambda1();
    double get_lambda2();
    uint64_t get_d();
    size_t get_n_epochs();
    unsigned int get_hash_type();
    unsigned int get_seed();
    bool get_inter();

    void set_model(DataTable*);
    void set_alpha(double);
    void set_beta(double);
    void set_lambda1(double);
    void set_lambda2(double);
    void set_d(uint64_t);
    void set_n_epochs(size_t);
    void set_inter(bool);
    void set_hash_type(unsigned int);
    void set_seed(unsigned int);
};

#endif
