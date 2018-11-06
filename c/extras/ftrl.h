//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <vector>
#include <string>
#include "types.h"
#include "datatable.h"
#include <random>
#include "py_datatable.h"
#include "utils.h"

typedef std::unique_ptr<double[]> DoublePtr;
typedef std::unique_ptr<uint64_t[]> Uint64Ptr;
#define REPORT_FREQUENCY 1000

class Ftrl {
  public:
    Ftrl(double, double, double, double, uint64_t, size_t, bool, unsigned int,
        unsigned int);
    void train(const DataTable*);
    dtptr test(const DataTable*);
    double predict(const Uint64Ptr&, size_t);
    void update(const Uint64Ptr&, size_t, double, bool);
    double logloss(double, bool);
    void hash(Uint64Ptr&, const DataTable*, size_t);
    void hash_numeric(Uint64Ptr&, const DataTable*, size_t);
    void hash_string(Uint64Ptr&, const DataTable*, size_t);
    void hash_murmur(Uint64Ptr&, const DataTable*, size_t);
    static double signum(double);
    static double sigmoid(double);
    static double bsigmoid(double, double);

  private:
    double a;
    double b;
    double l1;
    double l2;
    size_t n_features;
    size_t n_features_inter;
    uint64_t d;
    size_t n_epochs;
    bool inter;
    int64_t : 56;
    unsigned int hash_type;
    unsigned int seed;
    int64_t : 64;
    DoublePtr n;
    DoublePtr z;
    DoublePtr w;
};

uint64_t hash_double(double);
void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out );

DECLARE_FUNCTION(
  ftrl,
  "ftrl(self, dt_test, a=0.01, b=1.0, l1=0.0, l2=1.0, d=2**24, n_epochs=1, inter=0, hash_type=1)\n\n",
  dt_EXTRAS_FTRL_cc)

