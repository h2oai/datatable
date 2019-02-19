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
#include <random>
#include "models/aggregator.h"


namespace py {

static PKArgs args_aggregate(
  11, 0, 0, false, false,
  {
    "dt", "min_rows", "n_bins", "nx_bins", "ny_bins", "nd_max_bins",
    "max_dimensions", "seed", "progress_fn", "nthreads", "double_precision"
  },
  "aggregate",

R"(aggregate(dt, min_rows=500, n_bins=500, nx_bins=50, ny_bins=50,
nd_max_bins=500, max_dimensions=50, seed=0, progress_fn=None,
nthreads=0, double_precision=False)
--

Aggregate a datatable.

Parameters
----------
  dt: datatable
      Frame to be aggregated.
  min_rows: int
      Minimum number of rows a datatable should have to be aggregated. 
      If datatable has `nrows` that is less than `min_rows`, aggregation
      is bypassed, and all rows become exemplars.
  n_bins: int
      Number of bins for 1D aggregation.
  nx_bins: int
      Number of x bins for 2D aggregation.
  ny_bins: int
      Number of y bins for 2D aggregation.
  nd_max_bins: int
      Maximum number of exemplars for ND aggregation, not a hard limit.
  max_dimensions: int
      Number of columns at which start using the projection method.
  seed: int
      Seed to be used for the projection method.
  progress_fn: object
      Python function for progress reporting with the signature
      `progress_fn(progress, status_code)`, where:
      - `progress` is a float value that corresponds to the aggregation
        progress on a scale from 0 to 1;
      - `status_code` takes two values: `0` – in progress, `1` – completed.
  nthreads: int
      Number of OpenMP threads aggregator should use. `0` means 
      use all the threads.
  double_precision: bool
      Whether to use double precision arithmetic or not. 

Returns
-------
  A list `[dt_exemplars, dt_members]`, where 
  - `dt_exemplars` is the aggregated `dt` frame with additional `members_count`
    column, that specifies number of members for each exemplar.
  - `dt_members` is a one-column frame contaiing `exemplar_id` for each of
    the original rows in `dt`.
)"
);


/*
*  Read arguments from Python's `aggregate()` function and aggregate data
*  either with single or double precision. Return a list consisting
*  of two frames: `df_exemplars` and `df_members`.
*/
static oobj aggregate(const PKArgs& args) {
  size_t min_rows = 500;
  size_t n_bins = 500;
  size_t nx_bins = 50; 
  size_t ny_bins = 50;
  size_t nd_max_bins = 500;
  size_t max_dimensions = 50;
  unsigned int seed = 0;
  unsigned int nthreads = 0;
  bool double_precision = false;
  py::oobj progress_fn = py::None();

  bool undefined_dt = args[0].is_none_or_undefined();
  bool defined_min_rows = !args[1].is_none_or_undefined();
  bool defined_n_bins = !args[2].is_none_or_undefined();
  bool defined_nx_bins = !args[3].is_none_or_undefined();
  bool defined_ny_bins = !args[4].is_none_or_undefined();
  bool defined_nd_max_bins = !args[5].is_none_or_undefined();
  bool defined_max_dimensions = !args[6].is_none_or_undefined();
  bool defined_seed = !args[7].is_none_or_undefined();
  bool defined_progress_fn = !args[8].is_none_or_undefined();
  bool defined_nthreads = !args[9].is_none_or_undefined();
  bool defined_double_precision = !args[10].is_none_or_undefined();

  if (undefined_dt) {
    throw ValueError() << "`dt`, i.e. a datatable to aggregate, parameter is "
                       << "missing";
  }

  DataTable* dt = args[0].to_frame();

  if (defined_min_rows) {
    min_rows = args[1].to_size_t();
  }

  if (defined_n_bins) {
    n_bins = args[2].to_size_t();
  }

  if (defined_nx_bins) {
    nx_bins = args[3].to_size_t();
  }

  if (defined_ny_bins) {
    ny_bins = args[4].to_size_t();
  }

  if (defined_nd_max_bins) {
    nd_max_bins = args[5].to_size_t();
  }

  if (defined_max_dimensions) {
    max_dimensions = args[6].to_size_t();
  }

  if (defined_seed) {
    seed = static_cast<unsigned int>(args[7].to_size_t());
  }

  if (defined_progress_fn) {
    progress_fn = py::oobj(args[8]);
  }

  if (defined_nthreads) {
    nthreads = static_cast<unsigned int>(args[9].to_size_t());
  }

  if (defined_double_precision) {
    double_precision = args[10].to_bool_strict();
  }

  dtptr dt_members, dt_exemplars;
  std::unique_ptr<AggregatorBase> agg;
  if (double_precision) {
    agg = make_unique<Aggregator<double>>(min_rows, n_bins, nx_bins, ny_bins,
                                          nd_max_bins, max_dimensions, seed,
                                          progress_fn, nthreads
                                         );
  } else {
    agg = make_unique<Aggregator<float>>(min_rows, n_bins, nx_bins, ny_bins,
                                         nd_max_bins, max_dimensions, seed,
                                         progress_fn, nthreads
                                        );
  }

  agg->aggregate(dt, dt_exemplars, dt_members);
  py::oobj df_exemplars = py::oobj::from_new_reference(
                            py::Frame::from_datatable(dt_exemplars.release())
                          );
  py::oobj df_members = py::oobj::from_new_reference(
                          py::Frame::from_datatable(dt_members.release())
                        );

  // Return exemplars and members frames
  py::olist list(2);
  list.set(0, df_exemplars);
  list.set(1, df_members);
  return std::move(list);
}

}  // namespace py



/*
*  Destructor for the AggregatorBase class.
*/
AggregatorBase::~AggregatorBase() {}



void DatatableModule::init_methods_aggregate() {
  ADD_FN(&py::aggregate, py::args_aggregate);
}

