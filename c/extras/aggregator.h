// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <vector>
#include <string>
#include "datatable_check.h"
#include "rowindex.h"
#include "types.h"
#include "datatable.h"
#include "column.h"
#include <random>

class Aggregator {
  public:
    Aggregator(DataTable*);
    ~Aggregator();
    DataTable* aggregate(double, int32_t, int32_t, int32_t, int32_t, unsigned int);

  public:
    DataTable* dt_in;
    DataTable* dt_out;

  private:
    DataTable* create_dt_out();
    DataTable* aggregate_1d(double, int32_t);
    DataTable* aggregate_2d(double, int32_t, int32_t);
    DataTable* aggregate_nd(int32_t, unsigned int);
    void aggregate_1d_continuous(double, int32_t);
    void aggregate_2d_continuous(double, int32_t, int32_t);
    void aggregate_1d_categorical(int32_t);
    void aggregate_2d_categorical(int32_t, int32_t);
    void aggregate_2d_mixed(bool, double, int32_t, int32_t);

    void normalize_row(double*, int32_t);
    double calculate_distance(double*, double*, int64_t, double);
    void adjust_radius(int32_t, double&);
    double* generate_pmatrix(int32_t, unsigned int);
    void project_row(double*, int32_t, double*, int32_t);
};
