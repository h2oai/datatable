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
typedef std::unique_ptr<size_t[]> SizetPtr;
#define REPORT_FREQUENCY 1000

class Ftrl {
  public:
    Ftrl(double, double, double, double, size_t, size_t, bool, size_t);
    void train(const DataTable*);
    DataTablePtr test(const DataTable*);
    double predict(const SizetPtr&, size_t);
    void update(const SizetPtr&, size_t, double, bool);
    double logloss(double, bool);
    void hash(SizetPtr&, const DataTable*, int64_t);
    void hash_numeric(SizetPtr&, const DataTable*, int64_t);
    void hash_string(SizetPtr&, const DataTable*, int64_t);
    void hash_murmur(SizetPtr&, const DataTable*, int64_t);
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
    size_t d;
    size_t n_epochs;
    bool inter;
    size_t hash_type;
    DoublePtr n;
    DoublePtr z;
    DoublePtr w;
};


DECLARE_FUNCTION(
  ftrl,
  "ftrl(self, dt_test, a=0.01, b=1.0, l1=0.0, l2=1.0, d=2**24, n_epochs=1, inter=0, hash_type=1)\n\n",
  dt_EXTRAS_FTRL_cc)

