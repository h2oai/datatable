//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------

#define dt_EXTRAS_AGGREGATOR_cc
#include "extras/aggregator.h"
#include <stdlib.h>
#include "utils/omp.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"
#include "utils/pyobj.h"

PyObject* aggregate(PyObject*, PyObject* args) {
  int32_t n_bins, nx_bins, ny_bins, max_dimensions;
  unsigned int seed;
  PyObject* dt;

  if (!PyArg_ParseTuple(args, "OiiiiI:aggregate",
                        &dt, &n_bins, &nx_bins, &ny_bins, &max_dimensions, &seed)) return nullptr;

  DataTable* dt_in = PyObj(dt).as_datatable();
  DataTablePtr dt_members = nullptr;


  Aggregator agg(n_bins, nx_bins, ny_bins, max_dimensions, seed);
  dt_members = agg.aggregate(dt_in);

  return pydatatable::wrap(dt_members.release());
}


Aggregator::Aggregator(int32_t n_bins_in, int32_t nx_bins_in, int32_t ny_bins_in, int32_t max_dimensions_in, unsigned int seed_in) :
  n_bins(n_bins_in),
  nx_bins(nx_bins_in),
  ny_bins(ny_bins_in),
  max_dimensions(max_dimensions_in),
  seed(seed_in)
{
}


DataTablePtr Aggregator::aggregate(DataTable* dt) {
  DataTablePtr dt_members = nullptr;
  DataTablePtr dt_double = nullptr;
  Column** cols_double = dt::amalloc<Column*>(dt->ncols + 1);
  Column** cols_members = dt::amalloc<Column*>(static_cast<int64_t>(2));

  for (int32_t i = 0; i < dt->ncols; ++i) {
    LType ltype = info(dt->columns[i]->stype()).ltype();
    switch (ltype) {
      case LType::BOOL:
      case LType::INT:
      case LType::REAL: cols_double[i] = dt->columns[i]->cast(SType::FLOAT64); break;
      default:          cols_double[i] = dt->columns[i]->shallowcopy();
    }
  }

  cols_double[dt->ncols] = nullptr;
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

  // Setting up a table for weights
  DataTable* dt_weights;
  Column** cols_weights = dt::amalloc<Column*>(static_cast<int64_t>(2));
  cols_weights[0] = Column::new_data_column(SType::INT32, static_cast<int64_t>(gb_members.ngroups()));
  cols_weights[1] = nullptr;
  dt_weights = new DataTable(cols_weights);
  int32_t* d_weights = static_cast<int32_t*>(dt_weights->columns[0]->data_w());
  std::memset(d_weights, 0, static_cast<size_t>(gb_members.ngroups()) * sizeof(int32_t));

  // Setting up exemplar indices and weights
  for (size_t i = 0; i < gb_members.ngroups(); ++i) {
    exemplar_indices[i] = ri_members_indices[offsets[i]];
    d_weights[i] = offsets[i+1] - offsets[i];
  }

  // Applying exemplars row index and binding exemplars with the weights
  RowIndex ri_exemplars = RowIndex::from_array32(std::move(exemplar_indices));
  dt_exemplars->replace_rowindex(ri_exemplars);
  DataTable* dt[1];
  dt[0] = dt_weights;
  dt_exemplars->cbind(dt, 1);
  delete dt_weights;
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
  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_exemplars->columns[0]);
  const double* d_c0 = static_cast<const double*>(dt_exemplars->columns[0]->data());
  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  // TODO: handle the case when the column is constant, i.e. min = max
  double norm_factor = n_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (int32_t i = 0; i < dt_exemplars->nrows; ++i) {
    d_cg[i] = static_cast<int32_t>(norm_factor * (d_c0[i] - c0->min()));
  }
}


void Aggregator::group_2d_continuous(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_exemplars->columns[0]);
  RealColumn<double>* c1 = static_cast<RealColumn<double>*>(dt_exemplars->columns[1]);

  double* d_c0 = static_cast<double*>(dt_exemplars->columns[0]->data_w());
  double* d_c1 = static_cast<double*>(dt_exemplars->columns[1]->data_w());
  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  // TODO: handle the case when one of the columns or the both are constant, i.e. min = max
  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());
  double normy_factor = ny_bins * (1 - epsilon) / (c1->max() - c1->min());

  for (int32_t i = 0; i < dt_exemplars->nrows; ++i) {
    d_cg[i] = static_cast<int32_t>(normy_factor * (d_c1[i] - c1->min())) * nx_bins +
              static_cast<int32_t>(normx_factor * (d_c0[i] - c0->min()));
  }
}


void Aggregator::group_1d_categorical(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);

  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt_exemplars->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_cg[group_indices_0[j]] = static_cast<int32_t>(i);
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

  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  const int32_t* offsets1 = grpby1.offsets_r();

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_cg[group_indices_0[j]] = static_cast<int32_t>(i);
    }
  }

  for (size_t i = 0; i < grpby1.ngroups(); ++i) {
    for (int32_t j = offsets1[i]; j < offsets1[i+1]; ++j) {
      d_cg[group_indices_1[j]] += static_cast<int32_t>(grpby0.ngroups() * i);
    }
  }
}


void Aggregator::group_2d_mixed(bool cont_index, DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  arr32_t cols(1);

  cols[0] = !cont_index;
  Groupby grpby0;
  RowIndex ri0 = dt_exemplars->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_exemplars->columns[cont_index]);
  const double* d_c0 = static_cast<const double*>(dt_exemplars->columns[cont_index]->data());
  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_cg[group_indices_0[j]] = nx_bins * static_cast<int32_t>(i) +
                                 static_cast<int32_t>(normx_factor * (d_c0[group_indices_0[j]] - c0->min()));
    }
  }
}


// Leland's algorithm for N-dimensional aggregation, see [1-2] for more details
// [1] https://www.cs.uic.edu/~wilkinson/Publications/outliers.pdf
// [2] https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/h2o/data/Aggregator.java
void Aggregator::group_nd(DataTablePtr& dt_exemplars, DataTablePtr& dt_members) {
  size_t exemplar_id = 0;
  int64_t ndims = std::min(max_dimensions, static_cast<int32_t>(dt_exemplars->ncols));
  double* exemplar = new double[ndims];
  double* member = new double[ndims];
  double* pmatrix = nullptr;
  std::vector<double*> exemplars;
  double delta, radius = .025 * (dt_exemplars->ncols), distance = 0.0;
  int32_t* d_cg = static_cast<int32_t*>(dt_members->columns[0]->data_w());

  if (dt_exemplars->ncols > max_dimensions) {
    adjust_radius(dt_exemplars, radius);
    pmatrix = generate_pmatrix(dt_exemplars);
    project_row(dt_exemplars, exemplar, 0, pmatrix);
  } else {
    normalize_row(dt_exemplars, exemplar, 0);
  }

  delta = radius * radius;
  exemplars.push_back(exemplar);
  d_cg[0] = 0;

  for (int32_t i = 1; i < dt_exemplars->nrows; ++i) {
    double min_distance = std::numeric_limits<double>::max();
    if (dt_exemplars->ncols > max_dimensions) project_row(dt_exemplars, member, i, pmatrix);
    else normalize_row(dt_exemplars, member, i);

    for (size_t j = 0; j < exemplars.size(); ++j) {
      distance = calculate_distance(member, exemplars[j], ndims, delta);

      if (distance < min_distance) {
        min_distance = distance;
        exemplar_id = j;
        if (min_distance < delta) break;
      }
    }

    if (min_distance < delta) {
      d_cg[i] = static_cast<int32_t>(exemplar_id);
    } else {
      d_cg[i] = static_cast<int32_t>(exemplars.size());
      exemplars.push_back(member);
      member = new double[ndims];
    }
  }

  delete[] pmatrix;
  delete[] member;
  for (size_t i= 0; i < exemplars.size(); ++i) {
    delete[] exemplars[i];
  }
}


void Aggregator::adjust_radius(DataTablePtr& dt_exemplars, double& radius) {
  double diff = 0;
  for (int i = 0; i < dt_exemplars->ncols; ++i) {
    if (dt_exemplars->columns[i]->stype() == SType::FLOAT64) {
      RealColumn<double>* ci = static_cast<RealColumn<double>*>(dt_exemplars->columns[i]);
      diff += (ci->max() - ci->min()) * (ci->max() - ci->min());
    }
  }

  diff /= dt_exemplars->ncols;
  radius = .05 * log(max_dimensions);
  if (diff > 10000.0) radius *= .4;
}


double Aggregator::calculate_distance(double* e1, double* e2, int64_t ndims, double delta) {
  double sum = 0.0;
  int32_t n = 0;

  for (int i = 0; i < ndims; ++i) {
    if (ISNA<double>(e1[i]) || ISNA<double>(e2[i])) continue;
    ++n;
    sum += (e1[i] - e2[i]) * (e1[i] - e2[i]);
    if (sum > delta) return sum;
  }

  return sum * ndims / n;
}


void Aggregator::normalize_row(DataTablePtr& dt, double* r, int32_t row_id) {
  for (int32_t i = 0; i < dt->ncols; ++i) {
    if (dt->columns[i]->stype() == SType::FLOAT64) {
      RealColumn<double>* c = static_cast<RealColumn<double>*>(dt->columns[i]);
      const double* d_c = static_cast<const double*>(dt->columns[i]->data());
      r[i] =  (d_c[row_id] - c->min()) / (c->max() - c->min());
    } else {
      r[i] = 0;
    }
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
  for (int32_t i = 0; i < (dt_exemplars->ncols) * max_dimensions; ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


void Aggregator::project_row(DataTablePtr& dt_exemplars, double* r, int32_t row_id, double* pmatrix) {
  std::memset(r, 0, static_cast<size_t>(max_dimensions) * sizeof(double));

  for (int32_t i = 0; i < dt_exemplars->ncols; ++i) {
    if (dt_exemplars->columns[i]->stype() == SType::FLOAT64) {
      const double* d_c = static_cast<const double*>(dt_exemplars->columns[i]->data());
      if (!ISNA<double>(d_c[row_id])) {

        RealColumn<double>* c = static_cast<RealColumn<double>*> (dt_exemplars->columns[i]);
        for (int32_t j = 0; j < max_dimensions; ++j) {
          //TODO: handle missing values and do r[j] /= n normalization at the end
          r[j] +=  pmatrix[i * max_dimensions + j] * (d_c[row_id] - c->min()) / (c->max() - c->min());
        }

      }
    }
  }
}
