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
#include "py_datatable.h"

typedef std::unique_ptr<double[]> DoublePtr;
typedef std::unique_ptr<uint64_t[]> Uint64Ptr;
#define REPORT_FREQUENCY 1000

struct FtrlParams {
    double a;
    double b;
    double l1;
    double l2;
    uint64_t d;
    size_t n_epochs;
    unsigned int hash_type;
    unsigned int seed;
    bool inter;
    size_t : 56;
};


class Ftrl {
  private:
    // Datatable with `z` and `n` model values.
    dtptr dt_model;
    double* z;
    double* n;
    
    // Input to the model.
    FtrlParams fp;

    // Calculated during the learning process.
    size_t n_features;
    size_t n_inter_features;
    DoublePtr w;
    bool model_trained;
    size_t : 56;

  public:
    Ftrl(FtrlParams);

    static const std::vector<std::string> model_cols;
    static const FtrlParams fp_default;

    // Learning and predicting methods.
    bool is_trained();
    void fit(const DataTable*);
    dtptr predict(const DataTable*);
    double predict_row(const Uint64Ptr&, size_t);
    void update(const Uint64Ptr&, size_t, double, bool);
    void init_model();
    void create_model();

    // Learning helper methods.
    static double logloss(double, bool);
    static double signum(double);
    static double sigmoid(double);
    static double bsigmoid(double, double);

    // Hashing methods.
    uint64_t hash_string(const char *, size_t);
    static uint64_t hash_double(double);
    void hash_row(Uint64Ptr&, const DataTable*, size_t);

    // Getters and setters, some will invalidate the learning results.
    DataTable* get_model();
    size_t get_n_features();
    double get_a();
    double get_b();
    double get_l1();
    double get_l2();
    uint64_t get_d();
    size_t get_n_epochs();
    unsigned int get_hash_type();
    unsigned int get_seed();
    bool get_inter();
    void set_model(DataTable*);
    void set_a(double);
    void set_b(double);
    void set_l1(double);
    void set_l2(double);
    void set_d(uint64_t);
    void set_n_epochs(size_t);
    void set_inter(bool);
    void set_hash_type(unsigned int);
    void set_seed(unsigned int);
};

