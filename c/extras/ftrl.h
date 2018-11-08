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

class Ftrl {
  public:
    Ftrl(double, double, double, double, uint64_t, size_t, bool,
         unsigned int, unsigned int);

    void train(const DataTable*);
    dtptr test(const DataTable*);
    double predict(const Uint64Ptr&, size_t);
    void update(const Uint64Ptr&, size_t, double, bool);

    double logloss(double, bool);
    static double signum(double);
    static double sigmoid(double);
    static double bsigmoid(double, double);

    uint64_t hash_string(const char *, size_t);
    static uint64_t hash_double(double);
    void hash_row(Uint64Ptr&, const DataTable*, size_t);

  private:
    double a;
    double b;
    double l1;
    double l2;
    size_t n_features;
    size_t n_inter_features;
    uint64_t d;
    size_t n_epochs;
    unsigned int hash_type;
    unsigned int seed;
    DoublePtr n;
    DoublePtr z;
    DoublePtr w;
    bool inter;
    uint64_t : 56;
};
