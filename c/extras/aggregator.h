//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <vector>
#include <string>
#include "rowindex.h"
#include "types.h"
#include "datatable.h"
#include <random>
#include "py_datatable.h"


class Aggregator {
  public:
    Aggregator(int32_t, int32_t, int32_t, int32_t, unsigned int);
    DataTable* aggregate(DataTable*);
    static constexpr double epsilon = 1.0e-15;

  private:
    int32_t n_bins;
    int32_t nx_bins;
    int32_t ny_bins;
    int32_t max_dimensions;
    unsigned int seed;

    void group_1d(DataTable*, DataTable*);
    void group_2d(DataTable*, DataTable*);
    void group_nd(DataTable*, DataTable*);
    void group_1d_continuous(DataTable*, DataTable*);
    void group_2d_continuous(DataTable*, DataTable*);
    void group_1d_categorical(DataTable*, DataTable*);
    void group_2d_categorical(DataTable*, DataTable*);
    void group_2d_mixed(bool, DataTable*, DataTable*);
    void aggregate_exemplars(DataTable*, DataTable*);

    void normalize_row(DataTable*, double*, int32_t);
    double calculate_distance(double*, double*, int64_t, double);
    void adjust_radius(DataTable*, double&);
    double* generate_pmatrix(DataTable*);
    void project_row(DataTable*, double*, int32_t, double*);
};


DECLARE_FUNCTION(
  aggregate,
  "aggregate(self, n_bins=500, nx_bins=50, ny_bins=50, max_dimensions=50, seed=0)\n\n",
  dt_EXTRAS_AGGREGATOR_cc)
