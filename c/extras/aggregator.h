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
#include <extras/dt_ftrl.h>


struct ex {
  size_t id;
  doubleptr coords;
};
typedef std::unique_ptr<ex> ExPtr;

#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
#define PBSTEPS 100

class Aggregator {
  public:
    Aggregator(size_t, size_t, size_t, size_t, size_t, size_t,
               unsigned int, py::oobj, unsigned int);
    dtptr aggregate(DataTable*);
    static constexpr double epsilon = 1.0e-15;
    static void set_norm_coeffs(double&, double&, double, double, size_t);
    static void print_progress(double, int);

  private:
    size_t min_rows;
    size_t n_bins;
    size_t nx_bins;
    size_t ny_bins;
    size_t nd_max_bins;
    size_t max_dimensions;
    unsigned int seed;
    unsigned int nthreads;
    py::oobj progress_fn;

    // Grouping and aggregating methods
    void group_0d(const DataTable*, dtptr&);
    void group_1d(const dtptr&, dtptr&);
    void group_2d(const dtptr&, dtptr&);
    void group_nd(const dtptr&, dtptr&);
    void group_1d_continuous(const dtptr&, dtptr&);
    void group_2d_continuous(const dtptr&, dtptr&);
    void group_1d_categorical(const dtptr&, dtptr&);
    void group_2d_categorical(const dtptr&, dtptr&);
    template<typename T1, typename T2>
    void group_2d_categorical_str(const dtptr&, dtptr&);
    void group_2d_mixed(bool, const dtptr&, dtptr&);
    template<typename T>
    void group_2d_mixed_str(bool, const dtptr&, dtptr&);
    bool random_sampling(dtptr&, size_t, size_t);
    void aggregate_exemplars(DataTable*, dtptr&, bool);

    // Helper methods
    size_t get_nthreads(const dtptr&);
    doubleptr generate_pmatrix(const dtptr&);
    void normalize_row(const dtptr&, doubleptr&, size_t);
    void project_row(const dtptr&, doubleptr&, size_t, doubleptr&);
    double calculate_distance(doubleptr&, doubleptr&, size_t, double,
                              bool early_exit=true);
    void adjust_delta(double&, std::vector<ExPtr>&, std::vector<size_t>&,
                      size_t);
    void adjust_members(std::vector<size_t>&, dtptr&);
    size_t calculate_map(std::vector<size_t>&, size_t);
    void progress(double, int status_code=0);
};
