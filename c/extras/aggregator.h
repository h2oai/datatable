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

struct ex {
  int64_t id;
  double* coords;
};


class Aggregator {
  public:
    Aggregator(int32_t, int32_t, int32_t, int32_t, int32_t, unsigned int);
    DataTablePtr aggregate(DataTable*);
    static constexpr double epsilon = 1.0e-15;
    static void set_norm_coeffs(double&, double&, double, double, int32_t);

  private:
    int32_t n_bins;
    int32_t nx_bins;
    int32_t ny_bins;
    int32_t nd_bins;
    int32_t max_dimensions;
    unsigned int seed;

    void group_1d(DataTablePtr&, DataTablePtr&);
    void group_2d(DataTablePtr&, DataTablePtr&);
    void group_nd(DataTablePtr&, DataTablePtr&);
    void group_1d_continuous(DataTablePtr&, DataTablePtr&);
    void group_2d_continuous(DataTablePtr&, DataTablePtr&);
    void group_1d_categorical(DataTablePtr&, DataTablePtr&);
    void group_2d_categorical(DataTablePtr&, DataTablePtr&);
    void group_2d_mixed(bool, DataTablePtr&, DataTablePtr&);
    void aggregate_exemplars(DataTable*, DataTablePtr&);

    size_t calculate_map(std::vector<int64_t>&, size_t);
    void adjust_members(std::vector<int64_t>&, DataTablePtr&);
    void adjust_delta(double&, std::vector<ex>&, std::vector<int64_t>&, int64_t);
    void normalize_row(DataTablePtr&, double*, int32_t);
    double calculate_distance(double*, double*, int64_t, double, bool early_exit=true);
    void adjust_radius(DataTablePtr&, double&);
    double* generate_pmatrix(DataTablePtr&);
    void project_row(DataTablePtr&, double*, int32_t, double*);
};


DECLARE_FUNCTION(
  aggregate,
  "aggregate(self, n_bins=500, nx_bins=50, ny_bins=50, nd_bins = 500, max_dimensions=25, seed=0)\n\n",
  dt_EXTRAS_AGGREGATOR_cc)
