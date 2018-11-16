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

class FtrlModel {
  public:
    FtrlModel(double, double, double, double, uint64_t, size_t, bool,
         unsigned int, unsigned int);

    FtrlModel(unsigned int, unsigned int);

    // Learning and predicting methods.
    void fit(const DataTable*);
    dtptr predict(const DataTable*);
    double predict_row(const Uint64Ptr&, size_t);
    void update(const Uint64Ptr&, size_t, double, bool);

    // Learning helper methods.
    double logloss(double, bool);
    static double signum(double);
    static double sigmoid(double);
    static double bsigmoid(double, double);

    // Hashing methods.
    uint64_t hash_string(const char *, size_t);
    static uint64_t hash_double(double);
    void hash_row(Uint64Ptr&, const DataTable*, size_t);
    static const std::vector<std::string> model_cols;

    // Getters and setters, some will invalidate learning results.
    void set_model(DataTable*);
    DataTable* get_model(void);

    void set_a(double a_in) { a = a_in; dt_model = nullptr; }
    double get_a(void)      { return a; }
    void set_b(double b_in) { b = b_in; dt_model = nullptr; }
    double get_b(void)      { return b; }
    void set_l1(double l1_in) { l1 = l1_in; dt_model = nullptr; }
    double get_l1(void)       { return l1; }
    void set_l2(double l2_in) { l2 = l2_in; dt_model = nullptr; }
    double get_l2(void)       { return l2; }
    void set_d(uint64_t d_in) { d = d_in; dt_model = nullptr; }
    uint64_t get_d(void)      { return d; }
    void set_inter(bool inter_in) { inter = inter_in; dt_model = nullptr; }
    bool get_inter(void)          { return inter; }
    void set_hash_type(unsigned int hash_type_in) { hash_type = hash_type_in; dt_model = nullptr; }
    unsigned int get_hash_type(void)              { return hash_type; }
    void set_seed(unsigned int seed_in) { seed = seed_in; dt_model = nullptr; }
    unsigned int get_seed(void)         { return seed; }

    // Changing these values should not invalidate any results.
    size_t n_epochs;

  private:
    // Input to the model.
    double a;
    double b;
    double l1;
    double l2;
    uint64_t d;
    bool inter;
    size_t : 24;
    unsigned int hash_type;
    unsigned int seed;
    size_t : 32;

    // Calculated during the learning process.
    size_t n_features;
    size_t n_inter_features;
    double* z;
    double* n;
    DoublePtr w;
    dtptr dt_model;
};

