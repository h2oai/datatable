//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_EXTRAS_AGGREGATOR_cc
#include "extras/aggregator.h"
#include "frame/py_frame.h"
#include "py_utils.h"
#include "python/obj.h"
#include "rowindex.h"
#include "types.h"
#include "utils/parallel.h"
#include "utils/shared_mutex.h"


/*
*  Reading data from Python and passing it to the C++ aggregator.
*/
PyObject* aggregate(PyObject*, PyObject* args) {

  int32_t min_rows, n_bins, nx_bins, ny_bins, nd_max_bins, max_dimensions;
  unsigned int seed, nthreads;
  PyObject* arg1;
  PyObject* progress_fn;

  if (!PyArg_ParseTuple(args, "OiiiiiiIOI:aggregate", &arg1, &min_rows, &n_bins,
                        &nx_bins, &ny_bins, &nd_max_bins, &max_dimensions, &seed,
                        &progress_fn, &nthreads)) return nullptr;
  DataTable* dt = py::obj(arg1).to_frame();

  Aggregator agg(min_rows, n_bins, nx_bins, ny_bins, nd_max_bins, max_dimensions,
                 seed, progress_fn, nthreads);

  // dt_exemplars changes in-place with a new column added to the end of it
  DataTable* dt_members = agg.aggregate(dt).release();
  py::Frame* frame_members = py::Frame::from_datatable(dt_members);

  return frame_members;
}


/*
*  Setting up aggregation parameters.
*/
Aggregator::Aggregator(int32_t min_rows_in, int32_t n_bins_in, int32_t nx_bins_in,
                       int32_t ny_bins_in, int32_t nd_max_bins_in, int32_t max_dimensions_in,
                       unsigned int seed_in, PyObject* progress_fn_in, unsigned int nthreads_in) :
  min_rows(min_rows_in),
  n_bins(n_bins_in),
  nx_bins(nx_bins_in),
  ny_bins(ny_bins_in),
  nd_max_bins(nd_max_bins_in),
  max_dimensions(max_dimensions_in),
  seed(seed_in),
  nthreads(nthreads_in),
  progress_fn(progress_fn_in)
{
}


/*
*  Convert all the numeric values to double, do grouping and aggregation.
*/
DataTablePtr Aggregator::aggregate(DataTable* dt) {
  int32_t max_bins;
  bool was_sampled = false;
  progress(0.0);
  DataTablePtr dt_members = nullptr;
  Column** cols_members = dt::amalloc<Column*>(static_cast<int64_t>(2));

  cols_members[0] = Column::new_data_column(SType::INT32, dt->nrows);
  cols_members[1] = nullptr;
  dt_members = DataTablePtr(new DataTable(cols_members, {"exemplar_id"}));

  if (dt->nrows >= min_rows) {
    DataTablePtr dt_double = nullptr;
    Column** cols_double = dt::amalloc<Column*>(dt->ncols + 1);
    int32_t ncols = 0;
    int32_t n_na_bins = 0;

    for (int64_t i = 0; i < dt->ncols; ++i) {
      LType ltype = info(dt->columns[i]->stype()).ltype();
      switch (ltype) {
        case LType::BOOL:
        case LType::INT:
        case LType::REAL: {
                            cols_double[ncols] = dt->columns[i]->cast(SType::FLOAT64);
                            auto c = static_cast<RealColumn<double>*>(cols_double[ncols]);
                            c->min(); // Pre-generating stats
                            ncols++;
                            break;
                          }
        default:          if (dt->ncols < 3) cols_double[ncols++] = dt->columns[i]->shallowcopy();
      }
    }

    cols_double[ncols] = nullptr;
    dt_double = DataTablePtr(new DataTable(cols_double, nullptr));
    switch (dt_double->ncols) {
      case 0:  group_0d(dt, dt_members);
               max_bins = nd_max_bins;
               break;
      case 1:  group_1d(dt_double, dt_members);
               max_bins = n_bins;
               n_na_bins = 1;
               break;
      case 2:  group_2d(dt_double, dt_members);
               max_bins = nx_bins * ny_bins;
               n_na_bins = 3;
               break;
      default: group_nd(dt_double, dt_members);
               max_bins = nd_max_bins;
    }
    was_sampled = random_sampling(dt_members, max_bins, n_na_bins);
  } else {
    group_0d(dt, dt_members);
  }

  aggregate_exemplars(dt, dt_members, was_sampled); // modify dt in place
  progress(1.0, 1);
  return dt_members;
}


/*
*  Check how many exemplars we have got, if there is more than `max_bins+1`
*  (e.g. too many distinct categorical values) do random sampling.
*/
bool Aggregator::random_sampling(DataTablePtr& dt_members, int32_t max_bins, int32_t n_na_bins) {
  bool was_sampled = false;
  // Sorting `dt_members` to calculate total number of exemplars.
  arr32_t cols(1);
  cols[0] = 0;
  Groupby gb_members;
  RowIndex ri_members = dt_members->sortby(cols, &gb_members);

  // Do random sampling if there is too many exemplars, `n_na_bins` accounts
  // for the additional N/A bins that may appear during grouping.
  if (static_cast<int32_t>(gb_members.ngroups()) > max_bins + n_na_bins) {
    const int32_t* offsets = gb_members.offsets_r();
    const int32_t* ri_members_indices = ri_members.indices32();
    auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());

    // First, set all `exemplar_id`s to `N/A`.
    for (int32_t i = 0; i < dt_members->nrows; ++i) {
      d_members[i] = GETNA<int32_t>();
    }

    // Second, randomly select `max_bins` groups.
    if (!seed) {
      std::random_device rd;
      seed = rd();
    }
    srand(seed);
    int32_t k = 0;
    while (k < max_bins) {
      int32_t i = rand() % static_cast<int32_t>(gb_members.ngroups());
      if (ISNA<int32_t>(d_members[ri_members_indices[offsets[i]]])) {
        for (int32_t j = offsets[i]; j < offsets[i+1]; ++j) {
          d_members[ri_members_indices[j]] = k;
        }
        k++;
      }
    }
    dt_members->columns[0]->get_stats()->reset();
    was_sampled = true;
  }

  return was_sampled;
}


/*
*  Sort/group the members frame and set up the first member
*  in each group as an exemplar with the corresponding `members_count`,
*  that is essentially a number of members within the group.
*  If members were randomly sampled, those who got `exemplar_id = NA`
*  are ending up in the zero group, that is ignored and not included
*  in the aggregated frame.
*/
void Aggregator::aggregate_exemplars(DataTable* dt,
                                     DataTablePtr& dt_members,
                                     bool was_sampled) {
  // Setting up offsets and members row index.
  arr32_t cols(1);
  cols[0] = 0;
  Groupby gb_members;
  RowIndex ri_members = dt_members->sortby(cols, &gb_members);
  const int32_t* offsets = gb_members.offsets_r();
  size_t n_exemplars = gb_members.ngroups() - was_sampled;
  arr32_t exemplar_indices(n_exemplars);

  const int32_t* ri_members_indices = nullptr;
  arr32_t temp;
  if (ri_members.isarr32()) {
    ri_members_indices = ri_members.indices32();
  } else if (ri_members.isslice()) {
    temp.resize(static_cast<size_t>(dt_members->nrows));
    ri_members.extract_into(temp);
    ri_members_indices = temp.data();
  } else if (ri_members.isarr64()){
    throw ValueError() << "RI_ARR64 is not supported for the moment";
  }

  // Setting up a table for counts
  DataTable* dt_counts;
  Column** cols_counts = dt::amalloc<Column*>(static_cast<int64_t>(2));
  cols_counts[0] = Column::new_data_column(SType::INT32,
                                           static_cast<int64_t>(n_exemplars));
  cols_counts[1] = nullptr;
  dt_counts = new DataTable(cols_counts, {"members_count"});
  auto d_counts = static_cast<int32_t*>(dt_counts->columns[0]->data_w());
  std::memset(d_counts, 0, static_cast<size_t>(n_exemplars) * sizeof(int32_t));

  // Setting up exemplar indices and counts
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  for (size_t i = was_sampled; i < gb_members.ngroups(); ++i) {
    exemplar_indices[i - was_sampled] = ri_members_indices[static_cast<size_t>(offsets[i])];
    d_counts[i - was_sampled] = offsets[i+1] - offsets[i];
  }

  // Replacing group ids with the actual exemplar ids for 1D and 2D aggregations,
  // this is also needed for ND due to re-mapping.
  for (size_t i = was_sampled; i < gb_members.ngroups(); ++i) {
    for (size_t j = 0; j < static_cast<size_t>(d_counts[i - was_sampled]); ++j) {
      size_t member_shift = static_cast<size_t>(offsets[i]) + j;
      d_members[ri_members_indices[member_shift]] = static_cast<int32_t>(i - was_sampled);
    }
  }
  dt_members->columns[0]->get_stats()->reset();

  // Applying exemplars row index and binding exemplars with the counts.
  RowIndex ri_exemplars = RowIndex::from_array32(std::move(exemplar_indices));
  dt->replace_rowindex(ri_exemplars);
  std::vector<DataTable*> dts = { dt_counts };
  dt->cbind(dts);

  for (int64_t i = 0; i < dt->ncols-1; ++i) {
    dt->columns[i]->get_stats()->reset();
  }
  delete dt_counts;
}



/*
*  Do no groupping, i.e. all rows become exemplars.
*/
void Aggregator::group_0d(const DataTable* dt, DataTablePtr& dt_members) {
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  for (int32_t i = 0; i < dt->nrows; ++i) {
    d_members[i] = i;
  }
}


/*
*  Call an appropriate function for 1D grouping.
*/
void Aggregator::group_1d(const DataTablePtr& dt, DataTablePtr& dt_members) {
  LType ltype = info(dt->columns[0]->stype()).ltype();

  switch (ltype) {
    case LType::BOOL:
    case LType::INT:
    case LType::REAL:   group_1d_continuous(dt, dt_members); break;
    case LType::STRING: group_1d_categorical(dt, dt_members); break;
    default:            throw ValueError() << "Datatype is not supported";
  }
}


/*
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
void Aggregator::group_2d(const DataTablePtr& dt, DataTablePtr& dt_members) {
  LType ltype0 = info(dt->columns[0]->stype()).ltype();
  LType ltype1 = info(dt->columns[1]->stype()).ltype();

  switch (ltype0) {
    case LType::BOOL:
    case LType::INT:
    case LType::REAL:    switch (ltype1) {
                           case LType::BOOL:
                           case LType::INT:
                           case LType::REAL:   group_2d_continuous(dt, dt_members); break;
                           case LType::STRING: group_2d_mixed(0, dt, dt_members); break;
                           default:            throw ValueError() << "Datatype is not supported";
                         }
                         break;

    case LType::STRING:  switch (ltype1) {
                           case LType::BOOL:
                           case LType::INT:
                           case LType::REAL:   group_2d_mixed(1, dt, dt_members); break;
                           case LType::STRING: group_2d_categorical(dt, dt_members); break;
                           default:            throw ValueError() << "Datatype is not supported";
                         }
                         break;

    default:             throw ValueError() << "Datatype is not supported";
  }
}


/*
*  Do 1D grouping for a continuous column, i.e. 1D binning.
*/
void Aggregator::group_1d_continuous(const DataTablePtr& dt,
                                     DataTablePtr& dt_members) {
  auto c0 = static_cast<const RealColumn<double>*>(dt->columns[0]);
  const double* d_c0 = c0->elements_r();
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  double norm_factor, norm_shift;
  set_norm_coeffs(norm_factor, norm_shift, c0->min(), c0->max(), n_bins);

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt->nrows; ++i) {
    if (ISNA<double>(d_c0[i])) {
      d_members[i] = GETNA<int32_t>();
    } else {
      d_members[i] = static_cast<int32_t>(norm_factor * d_c0[i] + norm_shift);
    }
  }
}


/*
*  Do 2D grouping for two continuous columns, i.e. 2D binning.
*/
void Aggregator::group_2d_continuous(const DataTablePtr& dt,
                                     DataTablePtr& dt_members) {
  auto c0 = static_cast<const RealColumn<double>*>(dt->columns[0]);
  auto c1 = static_cast<const RealColumn<double>*>(dt->columns[1]);
  const double* d_c0 = c0->elements_r();
  const double* d_c1 = c1->elements_r();
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  double normx_factor, normx_shift;
  double normy_factor, normy_shift;
  set_norm_coeffs(normx_factor, normx_shift, c0->min(), c0->max(), nx_bins);
  set_norm_coeffs(normy_factor, normy_shift, c1->min(), c1->max(), ny_bins);

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt->nrows; ++i) {
    int32_t na_case = ISNA<double>(d_c0[i]) + 2 * ISNA<double>(d_c1[i]);
    if (na_case) {
      d_members[i] = -na_case;
    } else {
      d_members[i] = static_cast<int32_t>(normy_factor * d_c1[i] + normy_shift) * nx_bins +
                     static_cast<int32_t>(normx_factor * d_c0[i] + normx_shift);
    }
  }
}


/*
*  Do 1D grouping for a categorical column, i.e. just a `group by` operation.
*/
void Aggregator::group_1d_categorical(const DataTablePtr& dt,
                                      DataTablePtr& dt_members) {
  arr32_t cols(1);
  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_members[group_indices_0[j]] = static_cast<int32_t>(i);
    }
  }
}


/*
*  Detect string types for both categorical columns and do a corresponding call
*  to `group_2d_mixed_str`.
*/
void Aggregator::group_2d_categorical (const DataTablePtr& dt,
                                       DataTablePtr& dt_members) {
  switch (dt->columns[0]->stype()) {
    case SType::STR32:  switch (dt->columns[1]->stype()) {
                          case SType::STR32:  group_2d_categorical_str<uint32_t, uint32_t>(dt, dt_members); break;
                          case SType::STR64:  group_2d_categorical_str<uint32_t, uint64_t>(dt, dt_members); break;
                          default:            throw ValueError() << "Column types must be either STR32 or STR64";
                        }
                        break;

    case SType::STR64:  switch (dt->columns[1]->stype()) {
                          case SType::STR32:  group_2d_categorical_str<uint64_t, uint32_t>(dt, dt_members); break;
                          case SType::STR64:  group_2d_categorical_str<uint64_t, uint64_t>(dt, dt_members); break;
                          default:            throw ValueError() << "Column types must be either STR32 or STR64";
                        }
                        break;

    default:            throw ValueError() << "Column types must be either STR32 or STR64";
  }
}


/*
*  Do 2D grouping for two categorical columns, i.e. two `group by` operations,
*  and combine their results.
*/
template<typename T0, typename T1>
void Aggregator::group_2d_categorical_str(const DataTablePtr& dt,
                                          DataTablePtr& dt_members) {
  auto c0 = static_cast<const StringColumn<T0>*>(dt->columns[0]);
  auto c1 = static_cast<const StringColumn<T1>*>(dt->columns[1]);
  const T0* d_c0 = c0->offsets();
  const T1* d_c1 = c1->offsets();

  arr32_t cols(1);
  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  cols[0] = 1;
  Groupby grpby1;
  RowIndex ri1 = dt->sortby(cols, &grpby1);
  const int32_t* group_indices_1 = ri1.indices32();

  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  const int32_t* offsets1 = grpby1.offsets_r();

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    auto group_id = static_cast<int32_t>(i);
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      int32_t gi = group_indices_0[j];
      if (ISNA<T0>(d_c0[gi])) {
        d_members[gi] = GETNA<int32_t>();
      } else {
        d_members[gi] = group_id;
      }
    }
  }

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby1.ngroups(); ++i) {
    auto group_id = static_cast<int32_t>(grpby0.ngroups() * i);
    for (int32_t j = offsets1[i]; j < offsets1[i+1]; ++j) {
      int32_t gi = group_indices_1[j];
      int32_t na_case = ISNA<int32_t>(d_members[gi]) + 2 * ISNA<T1>(d_c1[gi]);
      if (na_case) {
        d_members[gi] = -na_case;
      } else {
        d_members[gi] += group_id;
      }
    }
  }
}


/*
*  Detect string type for a categorical column and do a corresponding call
*  to `group_2d_mixed_str`.
*/
void Aggregator::group_2d_mixed (bool cont_index, const DataTablePtr& dt,
                                 DataTablePtr& dt_members) {
  switch (dt->columns[!cont_index]->stype()) {
    case SType::STR32:  group_2d_mixed_str<uint32_t>(cont_index, dt, dt_members); break;
    case SType::STR64:  group_2d_mixed_str<uint64_t>(cont_index, dt, dt_members); break;
    default:            throw ValueError() << "Column type must be either STR32 or STR64";
  }
}


/*
*  Do 2D grouping for one continuous and one categorical string column,
*  i.e. 1D binning for the continuous column and a `group by`
*  operation for the categorical one.
*/
template<typename T>
void Aggregator::group_2d_mixed_str (bool cont_index, const DataTablePtr& dt,
                                     DataTablePtr& dt_members) {
  auto c_cat = static_cast<const StringColumn<T>*>(dt->columns[!cont_index]);
  const T* d_cat = c_cat->offsets();

  arr32_t cols(1);
  cols[0] = !cont_index;
  Groupby grpby;
  RowIndex ri_cat = dt->sortby(cols, &grpby);
  const int32_t* gi_cat = ri_cat.indices32();

  auto c_cont = static_cast<RealColumn<double>*>(dt->columns[cont_index]);
  auto d_cont = c_cont->elements_r();
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets_cat = grpby.offsets_r();


  double normx_factor, normx_shift;
  set_norm_coeffs(normx_factor, normx_shift, c_cont->min(), c_cont->max(), nx_bins);

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby.ngroups(); ++i) {
    int32_t group_cat_id = nx_bins * static_cast<int32_t>(i);
    for (int32_t j = offsets_cat[i]; j < offsets_cat[i+1]; ++j) {
      int32_t gi = gi_cat[j];
      int32_t na_case = ISNA<double>(d_cont[gi]) + 2 * ISNA<T>(d_cat[gi]);
      if (na_case) {
        d_members[gi] = -na_case;
      } else {
        d_members[gi] = group_cat_id +
                        static_cast<int32_t>(normx_factor * d_cont[gi] + normx_shift);
      }
    }
  }
}


/*
*  Do ND grouping in the general case. The initial `delta` (`delta = radius^2`)
*  is set to machine precision, so that we are gathering some initial exemplars.
*  When this `delta` starts getting us more exemplars than is set by `nd_max_bins`
*  do the following:
*  - find the mean distance between all the gathered exemplars;
*  - merge all the exemplars that are within half of this distance;
*  - adjust `delta` taking into account initial size of bubbles;
*  - store the merging info and use it in `adjust_members(...)`.
*
*  Another approach is to have a constant `delta` see `Develop` branch
*  https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/
*  h2o/data/Aggregator.java based on the estimates given at
*  https://mathoverflow.net/questions/308018/coverage-of-balls-on-random-points-in-
*  euclidean-space?answertab=active#tab-top i.e.
*
*  double radius2 = (d / 6.0) - 1.744 * sqrt(7.0 * d / 180.0);
*  double radius = (d > 4)? .5 * sqrt(radius2) : .5 / pow(100.0, 1.0 / d);
*  if (d > max_dimensions) {
*    radius /= 7.0;
*  }
*  double delta = radius * radius;
*
*  However, for some datasets this `delta` results in too many (e.g. thousands) or
*  too few (e.g. just one) exemplars.
*/
void Aggregator::group_nd(const DataTablePtr& dt, DataTablePtr& dt_members) {
  OmpExceptionManager oem;
  dt::shared_bmutex shmutex;
  auto ncols = static_cast<int32_t>(dt->ncols);
  int64_t ndims = std::min(max_dimensions, ncols);
  std::vector<ExPtr> exemplars;
  std::vector<int64_t> ids;
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  DoublePtr pmatrix = nullptr;
  if (ncols > max_dimensions) pmatrix = generate_pmatrix(dt);

  // Figuring out how many threads to use.
  int32_t nth = get_nthreads(dt);

  // Start with a very small `delta`, that is Euclidean distance squared.
  double delta = epsilon;

  #pragma omp parallel num_threads(nth)
  {
    int32_t ith = omp_get_thread_num();
    nth = omp_get_num_threads();
    int64_t rstep = (dt->nrows > nth * PBSTEPS)? dt->nrows / (nth * PBSTEPS) : 1;
    double distance;
    DoublePtr member = DoublePtr(new double[ndims]);

    try {
      // Main loop over all the rows
      for (int32_t i = ith; i < dt->nrows; i += nth) {
        bool is_exemplar = true;
        if (ncols > max_dimensions) project_row(dt, member, i, pmatrix);
        else normalize_row(dt, member, i);

        {
          dt::shared_lock<dt::shared_bmutex> lock(shmutex, /* exclusive = */ false);
          for (size_t j = 0; j < exemplars.size(); ++j) {
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
          ExPtr e = ExPtr(new ex{static_cast<int64_t>(ids.size()), std::move(member)});
          member = DoublePtr(new double[ndims]);
          ids.push_back(e->id);
          d_members[i] = static_cast<int32_t>(e->id);
          exemplars.push_back(std::move(e));

          if (exemplars.size() > static_cast<size_t>(nd_max_bins)) {
            adjust_delta(delta, exemplars, ids, ndims);
          }
        }

        #pragma omp master
        if ((i / nth) % rstep == 0) progress(static_cast<double>(i+1) / dt->nrows);
      } // End main loop over all the rows

    } catch (...) {
      oem.capture_exception();
    }
  }
  oem.rethrow_exception_if_any();
  adjust_members(ids, dt_members);
}


/*
 *  Figure out how many threads we need to run ND groupping.
 */
int32_t Aggregator::get_nthreads(const DataTablePtr& dt) {
  int32_t nth;
  if (nthreads) {
    nth = static_cast<int32_t>(nthreads);
  } else {
    nth = config::nthreads;
    if (nth > dt->nrows) nth = static_cast<int32_t>(dt->nrows);
  }
  return nth;
}

/*
*  Adjust `delta` (i.e. `radius^2`) based on the mean distance between
*  the gathered exemplars and merge all the exemplars within that distance.
*  Here we will just use an additional index `k` to map triangular matrix
*  into 1D array of distances. However, one can also use mapping from `k` to `(i,j)`:
*  i = n - 2 - floor(sqrt(-8 * k + 4 * n * (n - 1) - 7) / 2.0 - 0.5);
*  j = k + i + 1 - n * (n - 1) / 2 + (n - i) * ((n - i) - 1) / 2;
*  and mapping from `(i,j)` to `k`:
*  k = (2 * n - i - 1 ) * i / 2 + j
*/
void Aggregator::adjust_delta(double& delta, std::vector<ExPtr>& exemplars,
                              std::vector<int64_t>& ids, int64_t ndims) {
  size_t n = exemplars.size();
  size_t n_distances = (n * n - n) / 2;
  size_t k = 0;
  DoublePtr deltas(new double[n_distances]);
  double total_distance = 0.0;
  bool merge_only = false;

  for (size_t i = 0; i < n - 1; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      double distance = calculate_distance(exemplars[i]->coords,
                                     exemplars[j]->coords,
                                     ndims,
                                     delta,
                                     0);
      total_distance += sqrt(distance);
      deltas[k++] = distance;
      // This check is required in the case one thread had already modified `delta`,
      // but others used the old value and produced unnecessary exemplars.
      // In this case we only merge exemplars but don't change `delta`.
      if (distance < delta) merge_only = true;
    }
  }

  double delta_merge;
  if (merge_only) {
    delta_merge = delta;
  } else {
    delta_merge = pow(0.5 * total_distance / n_distances, 2);
    // Update delta, taking into account size of the initial bubble
    delta += delta_merge + 2 * sqrt(delta * delta_merge);
  }

  // Set exemplars that have to be merged to `nullptr`.
  k = 0;
  for (size_t i = 0; i < n - 1; ++i) {
    for (size_t j = i + 1; j < n; ++j) {
      if (deltas[k++] < delta_merge && exemplars[i] != nullptr && exemplars[j] != nullptr) {
        ids[static_cast<size_t>(exemplars[j]->id)] = exemplars[i]->id;
        exemplars[j] = nullptr;
      }
    }
  }

  // Remove all the `nullptr` exemplars from the vector.
  exemplars.erase(remove(begin(exemplars),
                  end(exemplars),
                  nullptr),
                  end(exemplars));
}


/*
*  Based on the merging info adjust the members information,
*  i.e. set which exemplar they belong to.
*/
void Aggregator::adjust_members(std::vector<int64_t>& ids,
                                DataTablePtr& dt_members) {

  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  size_t* map = new size_t[ids.size()];

  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] == static_cast<int64_t>(i)) {
      map[i] = i;
    } else {
      map[i] = calculate_map(ids, i);
    }
  }

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt_members->nrows; ++i) {
    d_members[i] = static_cast<int32_t>(map[d_members[i]]);
  }

}


/*
*  For each exemplar find the one it was merged to.
*/
size_t Aggregator::calculate_map(std::vector<int64_t>& ids, size_t id) {
  if (id == static_cast<size_t>(ids[id])){
    return id;
  } else {
    return calculate_map(ids, static_cast<size_t>(ids[id]));
  }
}


/*
*  Calculate distance between two vectors. If `early_exit` is set to `true`,
*  stop when the distance reaches `delta`.
*/
double Aggregator::calculate_distance(DoublePtr& e1, DoublePtr& e2,
                                      int64_t ndims, double delta,
                                      bool early_exit /*=true*/) {
  double sum = 0.0;
  int32_t n = 0;

  for (size_t i = 0; i < static_cast<size_t>(ndims); ++i) {
    if (ISNA<double>(e1[i]) || ISNA<double>(e2[i])) continue;
    ++n;
    sum += (e1[i] - e2[i]) * (e1[i] - e2[i]);
    if (early_exit && sum > delta) return sum; // i/n normalization here?
  }

  return sum * ndims / n;
}


/*
*  Normalize the row elements to [0,1).
*/
void Aggregator::normalize_row(const DataTablePtr& dt, DoublePtr& r, int32_t row_id) {
  for (int64_t i = 0; i < dt->ncols; ++i) {
    Column* c = dt->columns[i];
    auto c_real = static_cast<RealColumn<double>*>(c);
    const double* d_real = c_real->elements_r();
    double norm_factor, norm_shift;

    set_norm_coeffs(norm_factor, norm_shift, c_real->min(), c_real->max(), 1);
    r[static_cast<size_t>(i)] =  norm_factor * d_real[row_id] + norm_shift;
  }
}


/*
*  Generate projection matrix.
*/
DoublePtr Aggregator::generate_pmatrix(const DataTablePtr& dt_exemplars) {
  std::default_random_engine generator;
  DoublePtr pmatrix = DoublePtr(new double[(dt_exemplars->ncols) * max_dimensions]);

  if (!seed) {
    std::random_device rd;
    seed = rd();
  }

  generator.seed(seed);
  std::normal_distribution<double> distribution(0.0, 1.0);

  #pragma omp parallel for schedule(static)
  for (size_t i = 0;
    i < static_cast<size_t>((dt_exemplars->ncols) * max_dimensions); ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


/*
*  Project a particular row on a subspace by using the projection matrix.
*/
void Aggregator::project_row(const DataTablePtr& dt_exemplars, DoublePtr& r,
                             int32_t row_id, DoublePtr& pmatrix) {

  std::memset(r.get(), 0, static_cast<size_t>(max_dimensions) * sizeof(double));
  int32_t n = 0;
  for (size_t i = 0; i < static_cast<size_t>(dt_exemplars->ncols); ++i) {
    Column* c = dt_exemplars->columns[i];
    auto c_real = static_cast<RealColumn<double>*> (c);
    auto d_real = c_real->elements_r();

    if (!ISNA<double>(d_real[row_id])) {
      double norm_factor, norm_shift;
      set_norm_coeffs(norm_factor, norm_shift, c_real->min(), c_real->max(), 1);
      double norm_row = norm_factor * d_real[row_id] + norm_shift;
      for (size_t j = 0; j < static_cast<size_t>(max_dimensions); ++j) {
        r[j] +=  pmatrix[i * static_cast<size_t>(max_dimensions) + j] * norm_row;
      }
      ++n;
    }
  }
  for (size_t j = 0; j < static_cast<size_t>(max_dimensions); ++j) {
    r[j] /= n;
  }
}


/*
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
void Aggregator::set_norm_coeffs(double& norm_factor, double& norm_shift,
                                 double c_min, double c_max, int32_t c_bins) {
  if (fabs(c_max - c_min) > epsilon) {
    norm_factor = c_bins * (1 - epsilon) / (c_max - c_min);
    norm_shift = -norm_factor * c_min;
  } else {
    norm_factor = 0.0;
    norm_shift =  0.5 * c_bins;
  }
}


/*
*  Helper function to report on the aggregation process, clear when finished.
*/
void Aggregator::print_progress(double progress, int status_code) {
  int val = static_cast<int> (progress * 100);
  int lpad = static_cast<int> (progress * PBWIDTH);
  int rpad = PBWIDTH - lpad;
  printf("\rAggregating: [%.*s%*s] %3d%%", lpad, PBSTR, rpad, "", val);
  if (status_code) printf("\33[2K\r");
  fflush (stdout);
}


/*
*  Helper function to invoke the Python progress function if supplied,
*  otherwise just print the progress bar.
*/
void Aggregator::progress(double progress, int status_code /*= 0*/) {
  if (PyCallable_Check(progress_fn)) {
    PyObject_CallFunction(progress_fn,"di",progress,status_code);
  } else print_progress(progress, status_code);
}
