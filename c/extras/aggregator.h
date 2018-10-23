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
#include "utils.h"


typedef std::unique_ptr<double[]> DoublePtr;
struct ex {
  int64_t id;
  DoublePtr coords;
};
typedef std::unique_ptr<ex> ExPtr;

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
#define PBSTEPS 100

class Aggregator {
  public:
    Aggregator(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t,
               unsigned int, PyObject*, unsigned int);
    DataTablePtr aggregate(DataTable*);
    static constexpr double epsilon = 1.0e-15;
    static void set_norm_coeffs(double&, double&, double, double, int32_t);
    static void print_progress(double, int);

  private:
    int32_t min_rows;
    int32_t n_bins;
    int32_t nx_bins;
    int32_t ny_bins;
    int32_t nd_max_bins;
    int32_t max_dimensions;
    unsigned int seed;
    unsigned int nthreads;
    PyObject* progress_fn;

    // Grouping and aggregating methods
    void group_0d(const DataTable*, DataTablePtr&);
    void group_1d(const DataTablePtr&, DataTablePtr&);
    void group_2d(const DataTablePtr&, DataTablePtr&);
    void group_nd(const DataTablePtr&, DataTablePtr&);
    void group_1d_continuous(const DataTablePtr&, DataTablePtr&);
    void group_2d_continuous(const DataTablePtr&, DataTablePtr&);
    void group_1d_categorical(const DataTablePtr&, DataTablePtr&);
    void group_2d_categorical(const DataTablePtr&, DataTablePtr&);
    template<typename T1, typename T2>
    void group_2d_categorical_str(const DataTablePtr&, DataTablePtr&);
    void group_2d_mixed(bool, const DataTablePtr&, DataTablePtr&);
    template<typename T>
    void group_2d_mixed_str(bool, const DataTablePtr&, DataTablePtr&);
    bool random_sampling(DataTablePtr&, int32_t, int32_t);
    void aggregate_exemplars(DataTable*, DataTablePtr&, bool);

    // Helper methods
    int32_t get_nthreads(const DataTablePtr&);
    DoublePtr generate_pmatrix(const DataTablePtr&);
    void normalize_row(const DataTablePtr&, DoublePtr&, int32_t);
    void project_row(const DataTablePtr&, DoublePtr&, int32_t, DoublePtr&);
    double calculate_distance(DoublePtr&, DoublePtr&, int64_t, double,
                              bool early_exit=true);
    void adjust_delta(double&, std::vector<ExPtr>&, std::vector<int64_t>&,
                      int64_t);
    void adjust_members(std::vector<int64_t>&, DataTablePtr&);
    size_t calculate_map(std::vector<int64_t>&, size_t);
    void progress(double, int status_code=0);
};


DECLARE_FUNCTION(
  aggregate,
  "aggregate(self, min_rows=500, n_bins=500, nx_bins=50, ny_bins=50, nd_max_bins=500, max_dimensions=50, seed=0, progress_fn=None, nthreads=0)\n\n",
  dt_EXTRAS_AGGREGATOR_cc)
