//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_EXTRAS_AGGREGATOR_cc
#include "extras/aggregator.h"
#include <cstdlib>
#include "py_utils.h"
#include "python/obj.h"
#include "rowindex.h"
#include "types.h"
#include "utils/omp.h"


PyObject* aggregate(PyObject*, PyObject* args) {
  int32_t n_bins, nx_bins, ny_bins, nd_bins, max_dimensions;
  unsigned int seed;
  PyObject* arg1;

  if (!PyArg_ParseTuple(args, "OiiiiiI:aggregate", &dt, &n_bins, &nx_bins, &ny_bins,
                        &nd_bins, &max_dimensions, &seed)) return nullptr;

  DataTable* dt_in = py::obj(arg1).to_frame();
  DataTablePtr dt_members = nullptr;


  Aggregator agg(n_bins, nx_bins, ny_bins, nd_bins, max_dimensions, seed);
  dt_members = agg.aggregate(dt_in);

  return pydatatable::wrap(dt_members.release());
}


Aggregator::Aggregator(int32_t n_bins_in, int32_t nx_bins_in, int32_t ny_bins_in,
                       int32_t nd_bins_in, int32_t max_dimensions_in, unsigned int seed_in) :
  n_bins(n_bins_in),
  nx_bins(nx_bins_in),
  ny_bins(ny_bins_in),
  nd_bins(nd_bins_in),
  max_dimensions(max_dimensions_in),
  seed(seed_in)
{
}


DataTablePtr Aggregator::aggregate(DataTable* dt) {
  DataTablePtr dt_members = nullptr;
  DataTablePtr dt_double = nullptr;
  Column** cols_double = dt::amalloc<Column*>(dt->ncols + 1);
  Column** cols_members = dt::amalloc<Column*>(static_cast<int64_t>(2));
  int32_t ncols = 0;

  for (int64_t i = 0; i < dt->ncols; ++i) {
    LType ltype = info(dt->columns[i]->stype()).ltype();
    switch (ltype) {
      case LType::BOOL:
      case LType::INT:
      case LType::REAL: cols_double[ncols++] = dt->columns[i]->cast(SType::FLOAT64); break;
      default:          if (dt->ncols < 3) cols_double[ncols++] = dt->columns[i]->shallowcopy();
    }
  }

  cols_double[ncols] = nullptr;
  cols_members[0] = Column::new_data_column(SType::INT32, dt->nrows);
  cols_members[1] = nullptr;
  dt_double = DataTablePtr(new DataTable(cols_double));
  dt_members = DataTablePtr(new DataTable(cols_members));

  switch (dt_double->ncols) {
    case 1:  group_1d(dt_double, dt_members); break;
    case 2:  group_2d(dt_double, dt_members); break;
    default: group_nd(dt_double, dt_members);
  }

  aggregate_exemplars(dt, dt_members); // modify dt in place
  return dt_members;
}

void Aggregator::aggregate_exemplars(DataTable* dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);
  cols[0] = 0;

  // Setting up offsets and members row index
  Groupby gb_members;
  RowIndex ri_members = dt_members->sortby(cols, &gb_members);
  const int32_t* offsets = gb_members.offsets_r();
  arr32_t exemplar_indices(gb_members.ngroups());
  const int32_t* ri_members_indices = ri_members.indices32();
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  // Setting up a table for counts
  DataTable* dt_counts;
  Column** cols_counts = dt::amalloc<Column*>(static_cast<int64_t>(2));
  cols_counts[0] = Column::new_data_column(SType::INT32, static_cast<int64_t>(gb_members.ngroups()));
  cols_counts[1] = nullptr;
  dt_counts = new DataTable(cols_counts);
  auto d_counts = static_cast<int32_t*>(dt_counts->columns[0]->data_w());
  std::memset(d_counts, 0, static_cast<size_t>(gb_members.ngroups()) * sizeof(int32_t));

  // Setting up exemplar indices and counts
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < gb_members.ngroups(); ++i) {
    exemplar_indices[i] = ri_members_indices[offsets[i]];
    d_counts[i] = offsets[i+1] - offsets[i];
  }

  // Replacing group ids with the actual exemplar ids for 1D and 2D aggregations
  // After re-mapping also need this for ND
  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < gb_members.ngroups(); ++i) {
    for (size_t j = 0; j < static_cast<size_t>(d_counts[i]); ++j) {
      size_t member_shift = static_cast<size_t>(offsets[i]) + j;
      d_members[ri_members_indices[member_shift]] = static_cast<int32_t>(i);
    }
  }
  dt_members->columns[0]->get_stats()->reset();

  // Applying exemplars row index and binding exemplars with the counts
  RowIndex ri_exemplars = RowIndex::from_array32(std::move(exemplar_indices));
  dt_exemplars->replace_rowindex(ri_exemplars);
  DataTable* dt[1];
  dt[0] = dt_counts;
  dt_exemplars->cbind(dt, 1);

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt_exemplars->ncols-1; ++i) {
    dt_exemplars->columns[i]->get_stats()->reset();
  }
  delete dt_counts;
}


void Aggregator::group_1d(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  LType ltype = info(dt_exemplars->columns[0]->stype()).ltype();

  switch (ltype) {
    case LType::BOOL:
    case LType::INT:
    case LType::REAL:   group_1d_continuous(dt_exemplars, dt_members); break;
    case LType::STRING: group_1d_categorical(dt_exemplars, dt_members); break;
    default:            throw ValueError() << "Datatype is not supported";
  }
}


void Aggregator::group_2d(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  LType ltype0 = info(dt_exemplars->columns[0]->stype()).ltype();
  LType ltype1 = info(dt_exemplars->columns[1]->stype()).ltype();

  switch (ltype0) {
    case LType::BOOL:
    case LType::INT:
    case LType::REAL:   {
                           switch (ltype1) {
                             case LType::BOOL:
                             case LType::INT:
                             case LType::REAL:   group_2d_continuous(dt_exemplars, dt_members); break;
                             case LType::STRING: group_2d_mixed(0, dt_exemplars, dt_members); break;
                             default:            throw ValueError() << "Datatype is not supported";
                           }
                        }
                        break;

    case LType::STRING: {
                           switch (ltype1) {
                             case LType::BOOL:
                             case LType::INT:
                             case LType::REAL:   group_2d_mixed(1, dt_exemplars, dt_members); break;
                             case LType::STRING: group_2d_categorical(dt_exemplars, dt_members); break;
                             default:            throw ValueError() << "Datatype is not supported";
                           }
                        }
                        break;

    default:            throw ValueError() << "Datatype is not supported";
  }
}


void Aggregator::group_1d_continuous(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  auto c0 = static_cast<RealColumn<double>*>(dt_exemplars->columns[0]);
  const double* d_c0 = c0->elements_r();
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  double norm_factor, norm_shift;
  set_norm_coeffs(norm_factor, norm_shift, c0->min(), c0->max(), n_bins);

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt_exemplars->nrows; ++i) {
    double v0 = ISNA<double>(d_c0[i])? 0 : d_c0[i];
    d_members[i] = static_cast<int32_t>(norm_factor * v0 + norm_shift);
  }
}


void Aggregator::group_2d_continuous(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  auto c0 = static_cast<RealColumn<double>*>(dt_exemplars->columns[0]);
  auto c1 = static_cast<RealColumn<double>*>(dt_exemplars->columns[1]);
  double* d_c0 = c0->elements_w();
  double* d_c1 = c1->elements_w();
  auto d_groups = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  double normx_factor, normx_shift;
  double normy_factor, normy_shift;
  set_norm_coeffs(normx_factor, normx_shift, c0->min(), c0->max(), nx_bins);
  set_norm_coeffs(normy_factor, normy_shift, c1->min(), c1->max(), ny_bins);

  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt_exemplars->nrows; ++i) {
    double v0 = ISNA<double>(d_c0[i])? 0 : d_c0[i];
    double v1 = ISNA<double>(d_c1[i])? 0 : d_c1[i];
    d_groups[i] = static_cast<int32_t>(normy_factor * v1 + normy_shift) * nx_bins +
                  static_cast<int32_t>(normx_factor * v0 + normx_shift);
  }
}


void Aggregator::group_1d_categorical(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);

  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt_exemplars->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  auto d_groups = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    auto group_id = static_cast<int32_t>(i);
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_groups[group_indices_0[j]] = group_id;
    }
  }
}


void Aggregator::group_2d_categorical(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);

  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt_exemplars->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  cols[0] = 1;
  Groupby grpby1;
  RowIndex ri1 = dt_exemplars->sortby(cols, &grpby1);
  const int32_t* group_indices_1 = ri1.indices32();

  auto d_groups = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  const int32_t* offsets1 = grpby1.offsets_r();

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    auto group_id = static_cast<int32_t>(i);
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_groups[group_indices_0[j]] = group_id;
    }
  }

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby1.ngroups(); ++i) {
    auto group_id = static_cast<int32_t>(grpby0.ngroups() * i);
    for (int32_t j = offsets1[i]; j < offsets1[i+1]; ++j) {
      d_groups[group_indices_1[j]] += group_id;
    }
  }
}


void Aggregator::group_2d_mixed(bool cont_index, DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);

  cols[0] = !cont_index;
  Groupby grpby;
  RowIndex ri_cat = dt_exemplars->sortby(cols, &grpby);
  const int32_t* gi_cat = ri_cat.indices32();

  auto c_cont = static_cast<RealColumn<double>*>(dt_exemplars->columns[cont_index]);
  auto d_cont = c_cont->elements_r();
  auto d_groups = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets_cat = grpby.offsets_r();

  double normx_factor, normx_shift;
  set_norm_coeffs(normx_factor, normx_shift, c_cont->min(), c_cont->max(), nx_bins);

  #pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < grpby.ngroups(); ++i) {
    int32_t group_cat_id = nx_bins * static_cast<int32_t>(i);
    for (int32_t j = offsets_cat[i]; j < offsets_cat[i+1]; ++j) {
      double v_cont = ISNA<double>(d_cont[gi_cat[j]])? 0 : d_cont[gi_cat[j]];
      d_groups[gi_cat[j]] = group_cat_id +
                            static_cast<int32_t>(normx_factor * v_cont + normx_shift);
    }
  }
}


// N-dimensional aggregation, see [1-3] for more details
// [1] https://www.cs.uic.edu/~wilkinson/Publications/outliers.pdf
// [2] https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/h2o/data/Aggregator.java
// [3] https://mathoverflow.net/questions/308018/coverage-of-balls-on-random-points-in-euclidean-space?answertab=active#tab-top
void Aggregator::group_nd(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  auto d = static_cast<int32_t>(dt_exemplars->ncols);
  int64_t ndims = std::min(max_dimensions, d);
  double* exemplar = new double[ndims];
  double* member = new double[ndims];
  double* pmatrix = nullptr;
  std::vector<ex> exemplars;
  std::vector<int64_t> ids;
  double delta, distance = 0.0;
  auto d_members = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  double radius2 = (d / 6.0) - 1.744 * sqrt(7.0 * d / 180.0);
  double radius = (d > 4)? .5 * sqrt(radius2) : .5 / pow(100.0, 1.0 / d);

  if (dt_exemplars->ncols > max_dimensions) {
    radius /= 7.0;
    pmatrix = generate_pmatrix(dt_exemplars);
    project_row(dt_exemplars, exemplar, 0, pmatrix);
  } else {
    normalize_row(dt_exemplars, exemplar, 0);
  }

  delta = radius * radius;

  ex e = {0, exemplar};
  exemplars.push_back(e);
  ids.push_back(e.id);
  d_members[0] = static_cast<int32_t>(e.id);

  for (int32_t i = 1; i < dt_exemplars->nrows; ++i) {
    if (dt_exemplars->ncols > max_dimensions) project_row(dt_exemplars, member, i, pmatrix);
    else normalize_row(dt_exemplars, member, i);

    for (size_t j = 0; j < exemplars.size(); ++j) {
      distance = calculate_distance(member, exemplars[j].coords, ndims, delta);
      if (distance < delta) {
        d_members[i] = static_cast<int32_t>(exemplars[j].id);
        break;
      }
    }

    if (distance >= delta) {
      e = (ex) {static_cast<int64_t>(ids.size()), member};
      exemplars.push_back(e);
      ids.push_back(e.id);
      d_members[i] = static_cast<int32_t>(e.id);
      member = new double[ndims];
      if (exemplars.size() > static_cast<size_t>(nd_bins)) {
        adjust_delta(delta, exemplars, ids, ndims);
      }
    }
  }

  adjust_members(ids, dt_members);

  delete[] pmatrix;
  delete[] member;
  #pragma omp parallel for schedule(static)
  for (size_t i= 0; i < exemplars.size(); ++i) {
    delete[] exemplars[i].coords;
  }
}


void Aggregator::adjust_members(std::vector<int64_t>& ids, DataTablePtr& dt_members) {
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


size_t Aggregator::calculate_map(std::vector<int64_t>& ids, size_t id) {
  if (id == static_cast<size_t>(ids[id])){
    return id;
  } else {
    return calculate_map(ids, static_cast<size_t>(ids[id]));
  }
}


void Aggregator::adjust_delta(double& delta, std::vector<ex>& exemplars, std::vector<int64_t>& ids, int64_t ndims) {
  double min_distance = std::numeric_limits<double>::max();

  for (auto it1 = exemplars.begin(); it1 < exemplars.end() - 1; ++it1) {
    std::vector<ex>::iterator min_it = exemplars.end();
    for (auto it2 = it1 + 1; it2 < exemplars.end(); ++it2) {
      double distance = calculate_distance((*it1).coords, (*it2).coords, ndims, delta, it1 != exemplars.begin());
      if (distance < min_distance ) {
        if (it1 == exemplars.begin()){
          min_distance = distance;
          delta += min_distance;
        }
        min_it = it2;
      }
    }

    if (min_it != exemplars.end()) {
      ids[static_cast<size_t>((*min_it).id)] = (*it1).id;
      delete[] (*min_it).coords;
      exemplars.erase(min_it);
    }
  }
}


void Aggregator::adjust_radius(DataTablePtr& dt_exemplars, double& radius) {
  double diff = 0;
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt_exemplars->ncols; ++i) {
    Column* c = dt_exemplars->columns[i];
    auto c_double = static_cast<RealColumn<double>*>(c);
    if (!ISNA<double>(c_double->min())) {
      diff += (c_double->max() - c_double->min()) * (c_double->max() - c_double->min());
    }
  }

  diff /= dt_exemplars->ncols;
  radius = .05 * log(max_dimensions);
  if (diff > 10000.0) radius *= .25;
}


double Aggregator::calculate_distance(double* e1, double* e2, int64_t ndims, double delta, bool early_exit /*=true*/) {
  double sum = 0.0;
  int32_t n = 0;

  for (int i = 0; i < ndims; ++i) {
    if (ISNA<double>(e1[i]) || ISNA<double>(e2[i])) continue;
    ++n;
    sum += (e1[i] - e2[i]) * (e1[i] - e2[i]);
    if (early_exit && sum > delta) return sum;
  }

  return sum * ndims / n;
}


void Aggregator::normalize_row(DataTablePtr& dt, double* r, int32_t row_id) {
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < dt->ncols; ++i) {
    Column* c = dt->columns[i];
    auto c_real = static_cast<RealColumn<double>*>(c);
    const double* d_real = c_real->elements_r();
    double norm_factor, norm_shift;

    set_norm_coeffs(norm_factor, norm_shift, c_real->min(), c_real->max(), 1);
    r[i] =  norm_factor * d_real[row_id] + norm_shift;
  }
}


double* Aggregator::generate_pmatrix(DataTablePtr& dt_exemplars) {
  std::default_random_engine generator;
  double* pmatrix;

  if (!seed) {
    std::random_device rd;
    seed = rd();
  }

  generator.seed(seed);
  std::normal_distribution<double> distribution(0.0, 1.0);
  pmatrix = new double[(dt_exemplars->ncols) * max_dimensions];

//Can be enabled later when we don't care about exact reproducibility of the results
//#pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < (dt_exemplars->ncols) * max_dimensions; ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


void Aggregator::project_row(DataTablePtr& dt_exemplars, double* r, int32_t row_id, double* pmatrix) {
  std::memset(r, 0, static_cast<size_t>(max_dimensions) * sizeof(double));
  int32_t n = 0;

  for (int64_t i = 0; i < dt_exemplars->ncols; ++i) {
    Column* c = dt_exemplars->columns[i];
    auto c_real = static_cast<RealColumn<double>*> (c);
    auto d_real = c_real->elements_r();

    if (!ISNA<double>(d_real[row_id])) {
      double norm_factor, norm_shift;
      set_norm_coeffs(norm_factor, norm_shift, c_real->min(), c_real->max(), 1);
      double norm_row = norm_factor * d_real[row_id] + norm_shift;
      for (int32_t j = 0; j < max_dimensions; ++j) {
        r[j] +=  pmatrix[i * max_dimensions + j] * norm_row;
      }
      ++n;
    }
  }

  #pragma omp parallel for schedule(static)
  for (int32_t j = 0; j < max_dimensions; ++j) {
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
void Aggregator::set_norm_coeffs(double& norm_factor, double& norm_shift, double c_min, double c_max, int32_t c_bins) {
  if (fabs(c_max - c_min) > epsilon) {
    norm_factor = c_bins * (1 - epsilon) / (c_max - c_min);
    norm_shift = -norm_factor * c_min;
  } else {
    norm_factor = 0.0;
    norm_shift =  0.5 * c_bins;
  }
}
