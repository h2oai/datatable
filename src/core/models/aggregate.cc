//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "documentation.h"
#include "frame/py_frame.h"
#include "ltype.h"
#include "models/aggregate.h"
#include "models/column_caster.h"
#include "models/py_validator.h"
#include "models/utils.h"
#include "options.h"
#include "parallel/api.h"       // dt::parallel_for_static
#include "progress/work.h"      // dt::progress::work
#include "python/xargs.h"
#include "sort.h"


/**
 *  Read arguments from Python's `aggregate()` function and aggregate data
 *  either with single or double precision. Return a list consisting
 *  of two frames: `df_exemplars` and `df_members`.
 */
static py::oobj aggregate(const py::XArgs& args) {
  if (args[0].is_none()) {
    return py::None();
  }

  DataTable* dt = args[0].to_datatable();
  size_t min_rows = 500;
  size_t n_bins = 500;
  size_t nx_bins = 50;
  size_t ny_bins = 50;
  size_t nd_max_bins = 500;
  size_t max_dimensions = 50;
  unsigned int seed = 0;
  bool double_precision = false;
  double fixed_delta = std::numeric_limits<double>::quiet_NaN();

  bool defined_min_rows = !args[1].is_none_or_undefined();
  bool defined_n_bins = !args[2].is_none_or_undefined();
  bool defined_nx_bins = !args[3].is_none_or_undefined();
  bool defined_ny_bins = !args[4].is_none_or_undefined();
  bool defined_nd_max_bins = !args[5].is_none_or_undefined();
  bool defined_max_dimensions = !args[6].is_none_or_undefined();
  bool defined_seed = !args[7].is_none_or_undefined();
  bool defined_double_precision = !args[8].is_none_or_undefined();
  bool defined_fixed_radius = !args[9].is_none_or_undefined();

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

  if (defined_double_precision) {
    double_precision = args[8].to_bool_strict();
  }

  if (defined_fixed_radius) {
    double fixed_radius = args[9].to_double();
    py::Validator::check_positive(fixed_radius, args[9]);
    fixed_delta = fixed_radius * fixed_radius;
  }

  dtptr dt_members, dt_exemplars;
  std::unique_ptr<AggregatorBase> agg;
  size_t nrows = dt->nrows();
  if (double_precision) {
    agg = std::make_unique<Aggregator<double>>(min_rows, n_bins, nx_bins, ny_bins,
                                               nd_max_bins, max_dimensions, seed,
                                               nrows, fixed_delta);
  } else {
    agg = std::make_unique<Aggregator<float>>(min_rows, n_bins, nx_bins, ny_bins,
                                              nd_max_bins, max_dimensions, seed,
                                              nrows, fixed_delta);
  }

  agg->aggregate(dt, dt_exemplars, dt_members);
  py::oobj df_exemplars = py::Frame::oframe(dt_exemplars.release());
  py::oobj df_members = py::Frame::oframe(dt_members.release());

  // Return exemplars and members frames
  py::otuple tpl_out(2);
  tpl_out.set(0, df_exemplars);
  tpl_out.set(1, df_members);
  return std::move(tpl_out);
}

DECLARE_PYFN(&aggregate)
    ->name("aggregate")
    ->docs(dt::doc_models_aggregate)
    ->n_positional_args(1)
    ->n_required_args(1)
    ->n_keyword_args(9)
    ->arg_names({
      "frame", "min_rows", "n_bins", "nx_bins", "ny_bins", "nd_max_bins",
      "max_dimensions", "seed", "double_precision", "fixed_radius"
    });




/**
 *  Constructor, initialize all the input parameters.
 */
template <typename T>
Aggregator<T>::Aggregator(size_t min_rows_in, size_t n_bins_in,
                          size_t nx_bins_in, size_t ny_bins_in,
                          size_t nd_max_bins_in, size_t max_dimensions_in,
                          unsigned int seed_in, size_t nrows_in,
                          double fixed_delta_in) :
  dt(nullptr),
  min_rows(min_rows_in),
  n_bins(n_bins_in),
  nx_bins(nx_bins_in),
  ny_bins(ny_bins_in),
  nd_max_bins(nd_max_bins_in),
  max_dimensions(max_dimensions_in),
  fixed_delta(fixed_delta_in),
  seed(seed_in),
  nthreads(dt::nthreads_from_niters(nrows_in, MIN_ROWS_PER_THREAD))
{}

AggregatorBase::~AggregatorBase() {}



/**
 *  Main Aggregator method, convert all the numeric columns to `T`,
 *  do the corresponding grouping and final exemplar aggregation:
 *  - `dt` is an input datatable to aggregate;
 *  - `dt_exemplars` is the result of aggregation;
 *  - `dt_members` will store a relation between each of the original `dt` rows
 *     and the exemplars gathered in `dt_exemplars`.
 */
template <typename T>
void Aggregator<T>::aggregate(DataTable* dt_in,
                              dtptr& dt_exemplars_in,
                              dtptr& dt_members_in)
{
  dt::progress::work job(WORK_PREPARE + WORK_AGGREGATE + WORK_SAMPLE + WORK_FINALIZE);
  job.set_message("Preparing");
  dt = dt_in;
  bool needs_sampling;

  constexpr dt::SType agg_stype = sizeof(T) == 4? dt::SType::FLOAT32 : dt::SType::FLOAT64;


  Column col0 = Column::new_data_column(dt->nrows(), dt::SType::INT32);
  dt_members = dtptr(new DataTable({std::move(col0)}, {"exemplar_id"}));

  if (dt->nrows() >= min_rows && dt->nrows() != 0) {
    colvec catcols;
    Column contcol;
    size_t max_bins;

    contcols.reserve(dt->ncols());
    mins.reserve(dt->ncols());
    maxs.reserve(dt->ncols());


    size_t ncols = dt->ncols();

    // Create column convertors for numeric columns,
    // and a vector of categoricals.
    for (size_t i = 0; i < ncols; ++i) {
      bool is_continuous = true;
      const Column& col = dt->get_column(i);
      dt::SType col_stype = col.stype();
      switch (col_stype) {
        case dt::SType::VOID:
        case dt::SType::BOOL:
        case dt::SType::INT8:    contcol = make_inf2na_casted_column<int8_t, T>(col, agg_stype); break;
        case dt::SType::INT16:   contcol = make_inf2na_casted_column<int16_t, T>(col, agg_stype); break;
        case dt::SType::INT32:   contcol = make_inf2na_casted_column<int32_t, T>(col, agg_stype); break;
        case dt::SType::INT64:   contcol = make_inf2na_casted_column<int64_t, T>(col, agg_stype); break;
        case dt::SType::FLOAT32: contcol = make_inf2na_casted_column<float, T>(col, agg_stype); break;
        case dt::SType::FLOAT64: contcol = make_inf2na_casted_column<double, T>(col, agg_stype); break;
        case dt::SType::TIME64:
        case dt::SType::DATE32:  contcol = col.cast(agg_stype); break;
        case dt::SType::STR32:
        case dt::SType::STR64:   is_continuous = false;
                                 if (ncols < ND_COLS) {
                                   catcols.push_back(dt->get_column(i));
                                 }
                                 break;
        default:  throw TypeError() << "Columns with stype `"
                  << col_stype << "` are not supported";
      }
      if (is_continuous) {
        double min, max;
        contcol.stats()->get_stat(Stat::Min, &min);
        contcol.stats()->get_stat(Stat::Max, &max);
        mins.push_back(static_cast<T>(min));
        maxs.push_back(static_cast<T>(max));

        contcols.push_back(std::move(contcol));
      }
      size_t amount = (WORK_PREPARE * (i + 1)) / ncols;
      job.set_done_amount(amount);
    }

    size_t ncols_agg = contcols.size();
    if (catcols.size()) {
      dt_cat = dtptr(new DataTable(std::move(catcols), DataTable::default_names));
      ncols_agg += dt_cat->ncols();
    }

    // Depending on number of columns call a corresponding aggregating method.
    // If `dt` has too few rows, do not aggregate it, instead, just sort it by
    // the first column calling `group_0d()`.
    {
      job.set_message("Aggregating");
      dt::progress::subtask subjob(job, WORK_AGGREGATE);
      switch (ncols_agg) {
        case 0:  needs_sampling = group_0d();
                 max_bins = nd_max_bins;
                 break;
        case 1:  needs_sampling = group_1d();
                 max_bins = n_bins;
                 break;
        case 2:  needs_sampling = group_2d();
                 max_bins = nx_bins * ny_bins;
                 break;
        default: needs_sampling = group_nd();
                 max_bins = nd_max_bins;
      }
      subjob.done();
    }
    if (needs_sampling) {
      // Sample exemplars if we gathered too many.
      job.set_message("Sampling");
      dt::progress::subtask subjob(job, WORK_SAMPLE);
      needs_sampling = sample_exemplars(max_bins);
      subjob.done();
    } else {
      job.add_done_amount(WORK_SAMPLE);
    }
  } else {
    group_0d();
    needs_sampling = false;
    job.add_done_amount(WORK_PREPARE + WORK_AGGREGATE + WORK_SAMPLE);
  }

  // Do not aggregate `dt` in-place, instead, make a shallow copy
  // and apply rowindex based on the `exemplar_id`s gathered in `dt_members`.
  dt_exemplars = dtptr(new DataTable(*dt));
  {
    job.set_message("Finalizing");
    dt::progress::subtask subjob(job, WORK_FINALIZE);
    aggregate_exemplars(needs_sampling);
    subjob.done();
  }

  dt_exemplars_in = std::move(dt_exemplars);
  dt_members_in = std::move(dt_members);

  // We do not need a pointer to the original datatable anymore,
  // also clear vector of column convertors and datatable with categoricals.
  dt = nullptr;

  contcols.clear();
  mins.clear();
  maxs.clear();
  dt_cat = nullptr;
  job.done();
}


/**
 *  Check how many exemplars we have got, if there is more than `max_bins`
 *  (e.g. too many distinct categorical values) do random sampling.
 */
template <typename T>
bool Aggregator<T>::sample_exemplars(size_t max_bins) {
  bool was_sampled = false;

  // Sorting `dt_members` to calculate total number of exemplars.
  auto res = group({dt_members->get_column(0)},
                   {SortFlag::NONE});
  RowIndex ri_members = std::move(res.first);
  Groupby gb_members = std::move(res.second);

  // Do random sampling if there is too many exemplars.
  if (gb_members.size() > max_bins) {
    const int32_t* offsets = gb_members.offsets_r();
    auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());

    // First, set all `exemplar_id`s to `N/A`.
    dt::parallel_for_static(dt_members->nrows(), nthreads,
      [&](size_t i) {
        d_members[i] = dt::GETNA<int32_t>();
      });

    // Second, randomly select `max_bins` groups.
    if (!seed) {
      std::random_device rd;
      seed = rd();
    }
    srand(seed);
    size_t k = 0;
    dt::progress::work job(max_bins);
    while (k < max_bins) {
      int32_t i = rand() % static_cast<int32_t>(gb_members.size());
      size_t off_i = static_cast<size_t>(offsets[i]);
      size_t ri;
      bool rii_valid = ri_members.get_element(off_i, &ri);
      (void) rii_valid;
      xassert(rii_valid);
      if (dt::ISNA<int32_t>(d_members[ri])) {
        size_t off_i1 = static_cast<size_t>(offsets[i + 1]);
        dt::parallel_for_static(off_i1 - off_i,
          [&](size_t j) {
            rii_valid = ri_members.get_element(j + off_i, &ri);
            xassert(rii_valid);  (void) rii_valid;
            d_members[ri] = static_cast<int32_t>(k);
          });

        k++;
        job.add_done_amount(1);
      }
    }
    dt_members->get_column(0).reset_stats();
    job.done();
    was_sampled = true;
  }

  return was_sampled;
}


/**
 *  Sort/group the members frame and set up the first member
 *  in each group as an exemplar with the corresponding `members_count`,
 *  that is essentially a number of members within the group.
 *  If members were randomly sampled, those who got `exemplar_id == NA`
 *  are ending up in the zero group, that is ignored and not included
 *  in the aggregated frame.
 */
template <typename T>
void Aggregator<T>::aggregate_exemplars(bool was_sampled) {
  // Setting up offsets and members row index.
  auto res = group({dt_members->get_column(0)}, {SortFlag::NONE});

  RowIndex ri_members = std::move(res.first);
  Groupby gb_members = std::move(res.second);
  size_t ngroups = gb_members.size();
  const int32_t* offsets = gb_members.offsets_r();
  // If the input was an empty frame, then treat this case as if no
  // groups are present.
  if (offsets[ngroups] == 0) ngroups = 0;

  size_t n_exemplars = ngroups - was_sampled;
  Buffer exemplars_buf = Buffer::mem(n_exemplars * sizeof(int32_t));
  auto exemplar_indices = static_cast<int32_t*>(exemplars_buf.xptr());

  // Setting up a table for counts
  auto dt_counts = dtptr(new DataTable(
    {Column::new_data_column(n_exemplars, dt::SType::INT32)},
    {"members_count"}
  ));
  auto d_counts = static_cast<int32_t*>(dt_counts->get_column(0).get_data_editable());
  std::memset(d_counts, 0, n_exemplars * sizeof(int32_t));

  // Setting up exemplar indices and counts.
  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
  for (size_t i = was_sampled; i < ngroups; ++i) {
    size_t i_sampled = i - was_sampled;
    size_t off_i = static_cast<size_t>(offsets[i]);
    size_t rii;
    bool rii_valid = ri_members.get_element(off_i, &rii);
    xassert(rii_valid);  (void) rii_valid;
    exemplar_indices[i_sampled] = static_cast<int32_t>(rii);
    d_counts[i_sampled] = offsets[i+1] - offsets[i];
  }

  // Replacing aggregated exemplar_id's with group id's based on groupby,
  // because:
  // - for 1D and 2D some bins may be empty, and we want to exclude them;
  // - for ND we first generate exemplar_id's based on the exemplar row ids
  //   from the original dataset, so those should be replaced with the
  //   actual exemplar_id's from the exemplar column.
  dt::progress::work job(n_exemplars);
  dt::parallel_for_dynamic(n_exemplars,
    [&](size_t i_sampled) {
      size_t member_shift = static_cast<size_t>(offsets[i_sampled + was_sampled]);
      size_t jmax = static_cast<size_t>(d_counts[i_sampled]);
      for (size_t j = 0; j < jmax; ++j) {
        size_t rii;
        bool rii_valid = ri_members.get_element(member_shift + j, &rii);
        xassert(rii_valid);  (void) rii_valid;
        d_members[rii] = static_cast<int32_t>(i_sampled);
      }
      if (dt::this_thread_index() == 0) {
        job.set_done_amount(i_sampled);
      }
    });
  job.set_done_amount(n_exemplars);
  dt_members->get_column(0).reset_stats();

  // Applying exemplars row index and binding exemplars with the counts.
  RowIndex ri_exemplars = RowIndex(std::move(exemplars_buf), RowIndex::ARR32);
  dt_exemplars->apply_rowindex(ri_exemplars);
  std::vector<DataTable*> dts = { dt_counts.get() };
  dt_exemplars->cbind(dts);
  job.done();
}


/**
 *  Do no grouping, i.e. all rows become exemplars sorted by the first column.
 */
template <typename T>
bool Aggregator<T>::group_0d() {
  if (dt->ncols() > 0) {
    RowIndex ri_exemplars = group({dt->get_column(0)}, {SortFlag::SORT_ONLY}).first;
    auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
    ri_exemplars.iterate(0, dt->nrows(), 1,
      [&](size_t i, size_t j, bool jvalid) {
        if (!jvalid) return;
        d_members[j] = static_cast<int32_t>(i);
      });
  }
  return dt->nrows() > nd_max_bins;
}


/**
 *  Call an appropriate function for 1D grouping.
 */
template <typename T>
bool Aggregator<T>::group_1d() {
  size_t ncont = contcols.size();
  xassert(ncont < 2);

  bool res;
  if (ncont) {
    res = group_1d_continuous();
  }
  else {
    res = group_1d_categorical();
  }
  return res;
}


/**
 *  Call an appropriate function for 2D grouping.
 *  Dealing with NA's:
 *    - (value, NA) goes to bin -1;
 *    - (NA, value) goes to bin -2;
 *    - (NA, NA)    goes to bin -3.
 *  Rows having no NA's end up in the corresponding positive bins,
 *  so that we are not mixing NA and not NA members. After calling
 *  `aggregate_exemplars(...)` bins will be renumbered starting from 0,
 *  with NA bins (if ones exist) being gathered at the very beginning
 *  of the exemplar data frame.
 */
template <typename T>
bool Aggregator<T>::group_2d() {
  size_t ncont = contcols.size();
  xassert(ncont < 3);

  bool res;
  if (ncont == 0) {
    res = group_2d_categorical();
  }
  else if (ncont == 1) {
    res = group_2d_mixed();
  }
  else {
    res = group_2d_continuous();
  }
  return res;
}


/**
 *  1D binning for a continuous column.
 */
template <typename T>
bool Aggregator<T>::group_1d_continuous() {
  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
  T norm_factor, norm_shift;
  set_norm_coeffs(norm_factor, norm_shift, mins[0], maxs[0], n_bins);

  dt::parallel_for_static(contcols[0].nrows(),
    [&](size_t i) {
      T value;
      bool is_valid = contcols[0].get_element(i, &value);
      if (is_valid) {
        d_members[i] = static_cast<int32_t>(norm_factor * value + norm_shift);
      } else {
        d_members[i] = dt::GETNA<int32_t>();
      }
    });
  return false;
}


/**
 *  2D binning for two continuous columns.
 */
template <typename T>
bool Aggregator<T>::group_2d_continuous() {
  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());

  T normx_factor, normx_shift;
  T normy_factor, normy_shift;
  set_norm_coeffs(normx_factor, normx_shift, mins[0], maxs[0], nx_bins);
  set_norm_coeffs(normy_factor, normy_shift, mins[1], maxs[1], ny_bins);

  dt::parallel_for_static(contcols[0].nrows(),
    [&](size_t i) {
      T value0, value1;

      bool not_valid0 = !contcols[0].get_element(i, &value0);
      bool not_valid1 = !contcols[1].get_element(i, &value1);

      int32_t na_case = not_valid0 + 2 * not_valid1;
      if (na_case) {
        d_members[i] = -na_case;
      } else {
        d_members[i] = static_cast<int32_t>(normy_factor * value1 + normy_shift) *
                       static_cast<int32_t>(nx_bins) +
                       static_cast<int32_t>(normx_factor * value0 + normx_shift);
      }
    });
  return false;
}


/**
 *  1D grouping for a categorical column.
 */
template <typename T>
bool Aggregator<T>::group_1d_categorical() {
  auto col = dt_cat->get_column(0);
  xassert(col.ltype() == dt::LType::STRING);
  auto res = group({col}, {SortFlag::NONE});
  RowIndex ri = std::move(res.first);
  Groupby gb = std::move(res.second);
  auto offsets = gb.offsets_r();
  auto d_members = static_cast<int32_t*>(
                     dt_members->get_column(0).get_data_editable()
                   );

  // Check if we have an "NA" group
  bool na_group = false;
  {
    dt::CString val;
    size_t row;
    bool row_valid = ri.get_element(0, &row);
    (void) row_valid;
    xassert(row_valid);
    (void) row_valid;
    na_group = !col.get_element(row, &val);
  }

  dt::parallel_for_dynamic(gb.size(),
    [&](size_t i) {
      size_t offset_start = static_cast<size_t>(offsets[i]);
      size_t offset_end = static_cast<size_t>(offsets[i+1]);
      for (size_t j = offset_start; j < offset_end; ++j) {
        size_t row;
        bool row_valid = ri.get_element(j, &row);
        xassert(row_valid); (void)row_valid;
        d_members[row] = static_cast<int32_t>(i);
      }
    });
  return gb.size() > n_bins + na_group;
}



/**
 *  2D grouping for two categorical columns.
 */
template <typename T>
bool Aggregator<T>::group_2d_categorical() {
  const Column& col0 = dt_cat->get_column(0);
  const Column& col1 = dt_cat->get_column(1);
  xassert(col0.ltype() == dt::LType::STRING);
  xassert(col1.ltype() == dt::LType::STRING);

  auto res = group({col0, col1},
                   {SortFlag::NONE, SortFlag::NONE});
  RowIndex ri = std::move(res.first);
  Groupby gb = std::move(res.second);
  auto offsets = gb.offsets_r();
  auto d_members = static_cast<int32_t*>(
                     dt_members->get_column(0).get_data_editable()
                   );

  std::atomic<size_t> na_bin1 { 0 };
  std::atomic<size_t> na_bin2 { 0 };
  std::atomic<size_t> na_bin3 { 0 };
  dt::parallel_for_dynamic(gb.size(),
    [&](size_t i) {
      dt::CString val;
      auto offset_start = static_cast<size_t>(offsets[i]);
      auto offset_end = static_cast<size_t>(offsets[i+1]);

      // Check is we are dealing with the "NA" group
      size_t row;
      bool row_valid = ri.get_element(offset_start, &row);
      xassert(row_valid); (void)row_valid;
      bool val0_isna = !col0.get_element(row, &val);
      bool val1_isna = !col1.get_element(row, &val);
      size_t na_bin = val0_isna + 2 * val1_isna;

      auto group_id = -static_cast<int32_t>(na_bin);
      switch (na_bin) {
        case 1  : na_bin1++; break;
        case 2  : na_bin2++; break;
        case 3  : na_bin3++; break;
        default : group_id = static_cast<int32_t>(i);
      }

      for (size_t j = offset_start; j < offset_end; ++j) {
        row_valid = ri.get_element(j, &row);
        xassert(row_valid); (void)row_valid;
        d_members[row] = group_id;
      }
    });

    sztvec na_bins {na_bin1, na_bin2, na_bin3};
    size_t n_groups_merged = n_merged_nas(na_bins);
    size_t n_na_bins = (na_bin1 > 0) + (na_bin2 > 0) + (na_bin3 > 0);

    return gb.size() > nx_bins * ny_bins + n_na_bins + n_groups_merged;
}


/**
 *  Group one continuous and one categorical column.
 */
template <typename T>
bool Aggregator<T>::group_2d_mixed()
{
  const Column& col0 = dt_cat->get_column(0);
  xassert(col0.ltype() == dt::LType::STRING);

  auto res = group({col0}, {SortFlag::NONE});
  RowIndex ri = std::move(res.first);
  Groupby gb = std::move(res.second);

  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
  const int32_t* offsets_cat = gb.offsets_r();

  T normx_factor, normx_shift;
  set_norm_coeffs(normx_factor, normx_shift, mins[0], maxs[0], nx_bins);

  // Check if we have an "NA" group in the categorical column.
  bool na_cat_group = false;
  {
    dt::CString val;
    size_t row;
    bool row_valid = ri.get_element(0, &row);
    (void) row_valid;
    xassert(row_valid);
    (void) row_valid;
    na_cat_group = !col0.get_element(row, &val);
  }

  dt::parallel_for_static(gb.size(),
    [&](size_t i) {
      auto group_id_shift = static_cast<int32_t>(nx_bins * i);
      auto offset_start = static_cast<size_t>(offsets_cat[i]);
      auto offset_end = static_cast<size_t>(offsets_cat[i+1]);
      bool val0_isna = (i == 0) && na_cat_group;

      for (size_t j = offset_start; j < offset_end; ++j) {
        size_t row;
        bool row_valid = ri.get_element(j, &row);
        xassert(row_valid); (void)row_valid;

        T val1;
        bool val1_isna = !contcols[0].get_element(row, &val1);
        size_t na_bin = val1_isna + 2 * val0_isna;

        d_members[row] = na_bin? -static_cast<int32_t>(na_bin)
                               : group_id_shift +
                                 static_cast<int32_t>(normx_factor * val1 + normx_shift);
      }
    });

  // This condition is a good indicator that the resulting
  // exemplars need sampling. However, in some cases,
  // for instance, when the numeric column consists of
  // missing values only, it may be wrong. It's ok, because
  // the `sample_exemplars()` does a real check.
  return gb.size() > nx_bins + na_cat_group;
}


/**
 *  Calculate how many NA groups were merged together.
 */
template <typename T>
size_t Aggregator<T>::n_merged_nas(const sztvec& n_nas) {
  size_t n_merged_nas = 0;
  for (size_t i = 0; i < n_nas.size(); ++i) {
    n_merged_nas += (n_nas[i] > 0)? n_nas[i] - 1 : 0;
  }
  return n_merged_nas;
}


/**
 *  Do ND grouping in the general case.
 *
 *  We start with an empty exemplar list and do one pass through the data.
 *  If a partucular observation falls into a bubble with radius `r` and
 *  the center being one of the exemplars, we mark this observation
 *  as a member of that exemplar's cluster. If there is no exemplar found,
 *  the considered observation becomes a new exemplar.
 *
 *  First, the initial `delta`, i.e. `r^2`, is set to the machine precision,
 *  so that we can gather some initial exemplars. When the number
 *  of exemplars becomes larger than `nd_max_bins`, we do the `delta`
 *  adjustment as follows
 *
 *  - find the mean distance between all the gathered exemplars;
 *  - merge all the exemplars that are within the half of this distance;
 *  - adjust `delta` by taking into account the initial bubble radius;
 *  - store the exemplar's merging information to update member's
 *    in `adjust_members()`.
 *
 *  Another approach is to stick to the constant `delta`, however,
 *  for some datasets it may result in too many (e.g. thousands) or
 *  too few (e.g. just one) exemplars.
 */
template <typename T>
bool Aggregator<T>::group_nd() {
  dt::shared_bmutex shmutex;
  size_t ncols = contcols.size();
  size_t nrows = contcols[0].nrows();
  size_t ndims = std::min(max_dimensions, ncols);

  std::vector<exptr> exemplars; // Current exemplars
  sztvec ids; // All the exemplar's ids, including the merged ones
  sztvec coprimes;
  size_t nexemplars = 0;
  size_t ncoprimes = 0;

  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
  tptr<T> pmatrix;
  bool do_projection = ncols > max_dimensions;
  if (do_projection) pmatrix = generate_pmatrix(ncols);

  // Figuring out how many rows a thread will get.
  size_t nrows_per_thread = nrows / nthreads.get();

  T delta;
  size_t max_bins;

  if (_notnan(fixed_delta)) {
    delta = static_cast<T>(fixed_delta);
    // This will allow to use a fixed `delta` disabling `delta` adjustments
    max_bins = size_t(-1);
  } else {
    // Start with a very small `delta` if `fixed_delta` is not specified
    delta = epsilon;
    max_bins = nd_max_bins;
  }

  // Exemplar counter, if doesn't match thread local value, it means
  // some new exemplars were added (and may be even `delta` was adjusted)
  // meanwhile, so restart is needed for the `test_member` procedure.
  size_t ecounter = 0;

  dt::progress::work job(nrows_per_thread);
  dt::parallel_region(nthreads,
    [&] {
      size_t ith = dt::this_thread_index();
      size_t i0 = ith * nrows_per_thread;
      size_t i1 = (ith == nthreads.get() - 1)? nrows : i0 + nrows_per_thread;

      T distance;
      auto member = tptr<T>(new T[ndims]);
      size_t ecounter_local;

      // Each thread gets its own seed
      std::default_random_engine generator(seed + static_cast<unsigned int>(ith));

      // Main loop over all the rows
      for (size_t i = i0; i < i1; ++i) {
        bool is_exemplar = true;
        std::memset(member.get(), 0, ndims * sizeof(T));
        do_projection? project_row(member, i, ncols, pmatrix) :
                       normalize_row(member, i, ncols);

        test_member: {
          dt::shared_lock<dt::shared_bmutex> lock(shmutex, /* exclusive = */ false);
          ecounter_local = ecounter;

          // Generate random exemplar and coprime vector indices.
          // When `nexemplars` is zero, this may be any `size_t` number,
          // however, since we do not do any member testing in this case,
          // this is not an issue.
          std::uniform_int_distribution<size_t> exemplars_dist(0, nexemplars - 1);
          std::uniform_int_distribution<size_t> coprimes_dist(0, ncoprimes - 1);
          size_t exemplar_index = exemplars_dist(generator);
          size_t coprime_index = coprimes_dist(generator);

          // Instead of traversing exemplars in the order they appear
          // in the `exemplars` vector, we use modular quasi-random
          // paths. This ensures we get more uniform member distribution
          // across the clusters. Since `coprimes[coprime_index]` and
          // `nexemplars` are coprimes, `j` will take all the integer values
          // in the range [0; nexemplars - 1], where
          // - `exemplar_index` determines at which exemplar we start testing;
          // - `coprime_index` is a "seed" to the modular generator.
          for (size_t k = 0; k < nexemplars; ++k) {
            size_t j = (k * coprimes[coprime_index] + exemplar_index) % nexemplars;
            // Note, this distance will depend on delta, because
            // `early_exit = true` by default
            distance = calculate_distance(member, exemplars[j]->coords, ndims, delta);
            if (distance < delta) {
              d_members[i] = static_cast<int32_t>(exemplars[j]->id);
              is_exemplar = false;
              break;
            }
          }
        }

        if (is_exemplar) {
          dt::shared_lock<dt::shared_bmutex> lock(shmutex, /* exclusive = */ true);
          if (ecounter_local == ecounter) {
            ecounter++;
            exptr e = exptr(new exemplar{ids.size(), std::move(member)});
            member = tptr<T>(new T[ndims]);
            ids.push_back(e->id);
            d_members[i] = static_cast<int32_t>(e->id);
            exemplars.push_back(std::move(e));
            if (exemplars.size() > max_bins) {
              adjust_delta(delta, exemplars, ids, ndims);
            }
            coprimes = calculate_coprimes(exemplars.size());
            nexemplars = exemplars.size();
            ncoprimes = coprimes.size();
          } else {
            goto test_member;
          }
        }

        if (ith == 0) {
          job.set_done_amount(i - i0 + 1);
        }
      } // End main loop over all the rows
    });
  adjust_members(ids);
  job.done();
  return false;
}


/**
 *  Adjust `delta` (i.e. `radius^2`) based on the mean distance between
 *  the gathered exemplars and merge all the exemplars within that distance.
 *  Here we will just use an additional index `k` to map triangular matrix
 *  into 1D array of distances. However, one can also use mapping from `k` to `(i,j)`:
 *  i = n - 2 - floor(sqrt(-8 * k + 4 * n * (n - 1) - 7) / 2.0 - 0.5);
 *  j = k + i + 1 - n * (n - 1) / 2 + (n - i) * ((n - i) - 1) / 2;
 *  and mapping from `(i,j)` to `k`:
 *  k = (2 * n - i - 1 ) * i / 2 + j
 */
template <typename T>
void Aggregator<T>::adjust_delta(T& delta, std::vector<exptr>& exemplars,
                                 sztvec& ids, size_t ndims) {
  size_t n = exemplars.size();
  size_t n_distances = (n * n - n) / 2;
  size_t k = 0;
  tptr<T> deltas(new T[n_distances]);
  T total_distance = 0.0;

  for (size_t i = 0; i < n - 1; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      T distance = calculate_distance(exemplars[i]->coords,
                                      exemplars[j]->coords,
                                      ndims,
                                      delta,
                                      false);
      total_distance += sqrt(distance);
      deltas[k] = distance;
      k++;
    }
  }

  // Use `delta_merge` for merging exemplars.
  T delta_merge = pow(T(0.5) * total_distance / static_cast<T>(n_distances), T(2));

  // When exemplars are merged, all members will be within their `delta`,
  // not `delta_merge`. For that, update delta by taking into account size
  // of initial bubble.
  delta += delta_merge + 2 * sqrt(delta * delta_merge);


  // Set exemplars that have to be merged to `nullptr`.
  k = 0;
  for (size_t i = 0; i < n - 1; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      if (deltas[k] < delta_merge && exemplars[i] != nullptr && exemplars[j] != nullptr) {
        // Store merging information
        ids[exemplars[j]->id] = exemplars[i]->id;
        exemplars[j] = nullptr;
      }
      k++;
    }
  }

  // Remove all the `nullptr` exemplars from the vector.
  exemplars.erase(remove(begin(exemplars),
                  end(exemplars),
                  nullptr),
                  end(exemplars));
}


/**
 *  Based on the merging info adjust the members information,
 *  i.e. set which exemplar they belong to.
 */
template <typename T>
void Aggregator<T>::adjust_members(sztvec& ids) {
  auto d_members = static_cast<int32_t*>(dt_members->get_column(0).get_data_editable());
  auto map = std::unique_ptr<size_t[]>(new size_t[ids.size()]);
  auto nids = ids.size();

  dt::parallel_for_static(nids,
    [&](size_t i) {
      map[i] = (ids[i] == i) ? i : calculate_map(ids, i);
    });

  dt::parallel_for_static(dt_members->nrows(),
    [&](size_t i) {
      auto j = static_cast<size_t>(d_members[i]);
      d_members[i] = static_cast<int32_t>(map[j]);
    });

}


/**
 *  For each exemplar find the one it was merged to.
 */
template <typename T>
size_t Aggregator<T>::calculate_map(sztvec& ids, size_t id) {
  if (id == ids[id]) {
    return id;
  } else {
    return calculate_map(ids, ids[id]);
  }
}


/**
 *  Calculate distance between two vectors. If `early_exit` is set to `true`,
 *  stop when the distance reaches `delta`.
 */
template <typename T>
T Aggregator<T>::calculate_distance(tptr<T>& e1, tptr<T>& e2,
                                    size_t ndims, T delta,
                                    bool early_exit /* = true */) {
  T distance = 0;
  size_t n = 0;

  for (size_t i = 0; i < ndims; ++i) {
    if (dt::ISNA<T>(e1[i]) || dt::ISNA<T>(e2[i])) {
      continue;
    }
    ++n;
    distance += (e1[i] - e2[i]) * (e1[i] - e2[i]);
    if (early_exit && distance > delta) return distance; // i/n normalization here?
  }

  if (n != 0) {
    distance *= static_cast<T>(ndims) / static_cast<T>(n);
  }

  return distance;
}


/**
 *  Normalize the row elements to [0,1).
 */
template <typename T>
void Aggregator<T>::normalize_row(tptr<T>& r, size_t row, size_t ncols) {
  for (size_t i = 0; i < ncols; ++i) {
    T norm_factor, norm_shift, value;
    bool is_valid = contcols[i].get_element(row, &value);
    if (is_valid) {
      set_norm_coeffs(norm_factor, norm_shift, mins[i], maxs[i], 1);
      r[i] =  norm_factor * value + norm_shift;
    }
  }
}


/**
 *  Project a particular row on a subspace by using the projection matrix.
 */
template <typename T>
void Aggregator<T>::project_row(tptr<T>& r, size_t row, size_t ncols, tptr<T>& pmatrix)
{
  size_t n = 0;
  for (size_t i = 0; i < ncols; ++i) {
    T value;
    bool is_valid = contcols[i].get_element(row, &value);

    if (is_valid) {
      T norm_factor, norm_shift;
      set_norm_coeffs(norm_factor,
                      norm_shift,
                      mins[i],
                      maxs[i],
                      1);
      T norm_row = norm_factor * value + norm_shift;
      for (size_t j = 0; j < max_dimensions; ++j) {
        r[j] +=  pmatrix[i * max_dimensions + j] * norm_row;
      }
      ++n;
    }
  }
  if (n == 0) return;

  for (size_t j = 0; j < max_dimensions; ++j) {
    r[j] /= static_cast<T>(n);
  }
}


/**
 *  Generate projection matrix.
 */
template <typename T>
std::unique_ptr<T[]> Aggregator<T>::generate_pmatrix(size_t ncols) {
  std::default_random_engine generator;
  auto pmatrix = tptr<T>(new T[ncols * max_dimensions]);

  if (!seed) {
    std::random_device rd;
    seed = rd();
  }

  generator.seed(seed);
  std::normal_distribution<T> distribution(0.0, 1.0);

  for (size_t i = 0; i < ncols * max_dimensions; ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


/**
 *  To normalize a continuous column x to [0; 1] range we use
 *  the following formula: x_i_new = (x_i - min) / (max - min),
 *  where x_i is the value stored in the i-th row, max and min are the column
 *  maximum and minimum, respectively. To save on arithmetics, this can be
 *  easily converted to x_i_new = x_i * norm_factor + norm_shift,
 *  where norm_factor = 1 / (max - min) and norm_shift = - min / (max - min).
 *  When max = min, i.e. the column is constant, there is a singularity
 *  that may lead to wrong distance calculations done by the Aggregator.
 *  One of the approaches here is to handle the constant columns separately
 *  and set their values to 0.5, i.e. norm_factor = 0 and norm_shift = 0.5.
 */
template <typename T>
void Aggregator<T>::set_norm_coeffs(T& norm_factor, T& norm_shift,
                                    T c_min, T c_max, size_t c_bins) {
  if (std::fabs(c_max - c_min) > epsilon) {
    norm_factor = static_cast<T>(c_bins) * (1 - epsilon) / (c_max - c_min);
    norm_shift = -norm_factor * c_min;
  } else {
    norm_factor = T(0.0);
    norm_shift =  T(0.5) * static_cast<T>(c_bins);
  }
}



template class Aggregator<float>;
template class Aggregator<double>;
