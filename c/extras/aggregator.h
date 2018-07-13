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
    Aggregator(DataTable*);
    DataTablePtr aggregate(int32_t, int32_t, int32_t, int32_t, unsigned int);
    static constexpr double epsilon = 1.0e-15;

  private:
    DataTablePtr dt_out;
    DataTablePtr create_dt_out(DataTable*);
    void aggregate_1d(int32_t);
    void aggregate_2d(int32_t, int32_t);
    void aggregate_nd(int32_t, unsigned int);
    void aggregate_1d_continuous(int32_t);
    void aggregate_2d_continuous(int32_t, int32_t);
    void aggregate_1d_categorical(/*int32_t*/);
    void aggregate_2d_categorical(/*int32_t, int32_t*/);
    void aggregate_2d_mixed(bool, int32_t/*, int32_t*/);

    void normalize_row(double*, int32_t);
    double calculate_distance(double*, double*, int64_t, double);
    void adjust_radius(int32_t, double&);
    double* generate_pmatrix(int32_t, unsigned int);
    void project_row(double*, int32_t, double*, int32_t);
};


DECLARE_FUNCTION(
  aggregate,
  "aggregate()\n\n",
  dt_EXTRAS_AGGREGATOR_cc)
