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
  double epsilon;
  int32_t n_bins, nx_bins, ny_bins, max_dimensions;
  unsigned int seed;
  PyObject* arg0;

  if (!PyArg_ParseTuple(args, "OdiiiiI:aggregate",
                        &arg0, &epsilon, &n_bins, &nx_bins, &ny_bins, &max_dimensions, &seed)) return nullptr;

  DataTable* dt_in = PyObj(arg0).as_datatable();
  DataTablePtr dt_out;

  Aggregator agg(dt_in);
  dt_out = DataTablePtr(agg.aggregate(epsilon, n_bins, nx_bins, ny_bins, max_dimensions, seed));

  return pydatatable::wrap(dt_out.release());
}

Aggregator::Aggregator(DataTable* dt_in){
  create_dt_out(dt_in);
}

DataTablePtr Aggregator::create_dt_out(DataTable* dt_in) {
  Column** cols = dt::amalloc<Column*>(dt_in->ncols + 2);

  for (int32_t i = 0; i < dt_in->ncols; ++i) {
    LType ltype = stype_info[dt_in->columns[i]->stype()].ltype;
    switch (ltype) {
      case LT_BOOLEAN:
      case LT_INTEGER:
      case LT_REAL:    cols[i] = dt_in->columns[i]->cast(ST_REAL_F8); break;
      default:         cols[i] = dt_in->columns[i]->shallowcopy();
    }
  }

  cols[dt_in->ncols] = Column::new_data_column(ST_INTEGER_I4, dt_in->nrows);
  cols[dt_in->ncols + 1] = nullptr;
  dt_out = DataTablePtr(new DataTable(cols));

  return nullptr;
}


DataTablePtr Aggregator::aggregate(double epsilon, int32_t n_bins, int32_t nx_bins, int32_t ny_bins, int32_t max_dimensions, unsigned int seed) {
  switch (dt_out->ncols) {
    case 2:  aggregate_1d(epsilon, n_bins); break;
    case 3:  aggregate_2d(epsilon, nx_bins, ny_bins); break;
    default: aggregate_nd(max_dimensions, seed);
  }

  return std::move(dt_out);
}


void Aggregator::aggregate_1d(double epsilon, int32_t n_bins) {
  LType ltype = stype_info[dt_out->columns[0]->stype()].ltype;

  switch (ltype) {
    case LT_BOOLEAN:
    case LT_INTEGER:
    case LT_REAL:     aggregate_1d_continuous(epsilon, n_bins); break;
    case LT_STRING:   aggregate_1d_categorical(/*n_bins*/); break;
    default:          throw ValueError() << "Datatype is not supported";
  }
}


void Aggregator::aggregate_2d(double epsilon, int32_t nx_bins, int32_t ny_bins) {
  LType ltype0 = stype_info[dt_out->columns[0]->stype()].ltype;
  LType ltype1 = stype_info[dt_out->columns[1]->stype()].ltype;

  switch (ltype0) {
    case LT_BOOLEAN:
    case LT_INTEGER:
    case LT_REAL:    {
                        switch (ltype1) {
                          case LT_BOOLEAN:
                          case LT_INTEGER:
                          case LT_REAL:    aggregate_2d_continuous(epsilon, nx_bins, ny_bins); break;
                          case LT_STRING:  aggregate_2d_mixed(0, epsilon, nx_bins/*, ny_bins*/); break;
                          default:         throw ValueError() << "Datatype is not supported";
                        }
                      }
                      break;

    case LT_STRING:  {
                        switch (ltype1) {
                          case LT_BOOLEAN:
                          case LT_INTEGER:
                          case LT_REAL:    aggregate_2d_mixed(1, epsilon, nx_bins/*, ny_bins*/); break;
                          case LT_STRING:  aggregate_2d_categorical(/*nx_bins, ny_bins*/); break;
                          default:         throw ValueError() << "Datatype is not supported";
                        }
                      }
                      break;

    default:          throw ValueError() << "Datatype is not supported";
  }
}


void Aggregator::aggregate_1d_continuous(double epsilon, int32_t n_bins) {
  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_out->columns[0]);
  const double* d_c0 = static_cast<const double*>(dt_out->columns[0]->data());
  int32_t* d_c1 = static_cast<int32_t*>(dt_out->columns[1]->data_w());

//TODO: handle the case when the column is constant, i.e. min = max
  double norm_factor = n_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (int32_t i = 0; i < dt_out->nrows; ++i) {
    d_c1[i] = static_cast<int32_t>(norm_factor * (d_c0[i] - c0->min()));
  }
}


void Aggregator::aggregate_2d_continuous(double epsilon, int32_t nx_bins, int32_t ny_bins) {
  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_out->columns[0]);
  RealColumn<double>* c1 = static_cast<RealColumn<double>*>(dt_out->columns[1]);
  double* d_c0 = static_cast<double*>(dt_out->columns[0]->data_w());
  double* d_c1 = static_cast<double*>(dt_out->columns[1]->data_w());
  int32_t* d_c2 = static_cast<int32_t*>(dt_out->columns[2]->data_w());

//TODO: handle the case when one of the columns or the both are constant, i.e. min = max
  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());
  double normy_factor = ny_bins * (1 - epsilon) / (c1->max() - c1->min());

  for (int32_t i = 0; i < dt_out->nrows; ++i) {
    d_c2[i] = static_cast<int32_t>(normy_factor * (d_c1[i] - c1->min())) * nx_bins +
              static_cast<int32_t>(normx_factor * (d_c0[i] - c0->min()));
  }
}


void Aggregator::aggregate_1d_categorical(/*int32_t n_bins*/) {
  arr32_t cols(1);

  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt_out->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  int32_t* d_c1 = static_cast<int32_t*>(dt_out->columns[1]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_c1[group_indices_0[j]] = static_cast<int32_t>(i);
    }
  }
// TODO: store ri somehow to be reused for groupping with dt->replace_rowindex(ri);
}


void Aggregator::aggregate_2d_categorical(/*int32_t nx_bins, int32_t ny_bins*/) {
  arr32_t cols(1);

  cols[0] = 0;
  Groupby grpby0;
  RowIndex ri0 = dt_out->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  cols[0] = 1;
  Groupby grpby1;
  RowIndex ri1 = dt_out->sortby(cols, &grpby1);
  const int32_t* group_indices_1 = ri1.indices32();

  int32_t* d_c2 = static_cast<int32_t*>(dt_out->columns[2]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  const int32_t* offsets1 = grpby1.offsets_r();

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_c2[group_indices_0[j]] = static_cast<int32_t>(i);
    }
  }

  for (size_t i = 0; i < grpby1.ngroups(); ++i) {
    for (int32_t j = offsets1[i]; j < offsets1[i+1]; ++j) {
      d_c2[group_indices_1[j]] += static_cast<int32_t>(grpby0.ngroups() * i);
    }
  }
//TODO: store ri1 and ri2 somehow to be reused for groupping with dt->replace_rowindex(ri);
}


void Aggregator::aggregate_2d_mixed(bool cont_index, double epsilon, int32_t nx_bins/*, int32_t ny_bins*/) {
  arr32_t cols(1);

  cols[0] = !cont_index;
  Groupby grpby0;
  RowIndex ri0 = dt_out->sortby(cols, &grpby0);
  const int32_t* group_indices_0 = ri0.indices32();

  RealColumn<double>* c0 = static_cast<RealColumn<double>*>(dt_out->columns[cont_index]);
  const double* d_c0 = static_cast<const double*>(dt_out->columns[cont_index]->data());
  int32_t* d_c2 = static_cast<int32_t*>(dt_out->columns[2]->data_w());
  const int32_t* offsets0 = grpby0.offsets_r();
  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (size_t i = 0; i < grpby0.ngroups(); ++i) {
    for (int32_t j = offsets0[i]; j < offsets0[i+1]; ++j) {
      d_c2[group_indices_0[j]] = nx_bins * static_cast<int32_t>(i) +
                                 static_cast<int32_t>(normx_factor * (d_c0[group_indices_0[j]] - c0->min()));
    }
  }
}


// Leland's algorithm for N-dimensional aggregation, see [1-2] for more details
// [1] https://www.cs.uic.edu/~wilkinson/Publications/outliers.pdf
// [2] https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/h2o/data/Aggregator.java
void Aggregator::aggregate_nd(int32_t mcols, unsigned int seed) {
  size_t exemplar_id = 0;
  int64_t ndims = std::min(mcols, static_cast<int32_t>(dt_out->ncols - 1));
  double* exemplar = new double[ndims];
  double* member = new double[ndims];
  double* pmatrix = nullptr;
  std::vector<double*> exemplars;
  double delta, radius = .025 * (dt_out->ncols - 1), distance = 0.0;
  int32_t* d_cn = static_cast<int32_t*>(dt_out->columns[dt_out->ncols - 1]->data_w());

  if (dt_out->ncols - 1 > mcols) {
    adjust_radius(mcols, radius);
    pmatrix = generate_pmatrix(mcols, seed);
    project_row(exemplar, 0, pmatrix, mcols);
  } else normalize_row(exemplar, 0);

  delta = radius * radius;
  exemplars.push_back(exemplar);
  d_cn[0] = 0;

  for (int32_t i = 1; i < dt_out->nrows; ++i) {
    double min_distance = std::numeric_limits<double>::max();
    if (dt_out->ncols - 1 > mcols) project_row(member, i, pmatrix, mcols);
    else normalize_row(member, i);

    for (size_t j = 0; j < exemplars.size(); ++j) {
      distance = calculate_distance(member, exemplars[j], ndims, delta);

      if (distance < min_distance) {
        min_distance = distance;
        exemplar_id = j;
        if (min_distance < delta) break;
      }
    }

    if (min_distance < delta) {
      d_cn[i] = static_cast<int32_t>(exemplar_id);
    } else {
      d_cn[i] = static_cast<int32_t>(exemplars.size());
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


void Aggregator::adjust_radius(int32_t mcols, double& radius) {
  double diff = 0;
  for (int i = 0; i < dt_out->ncols - 1; ++i) {
    RealColumn<double>* ci = static_cast<RealColumn<double>*>(dt_out->columns[i]);
    diff += (ci->max() - ci->min()) * (ci->max() - ci->min());
  }

  diff /= dt_out->ncols - 1;
  radius = .05 * log(mcols);
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


void Aggregator::normalize_row(double* r, int32_t row_id) {
  for (int32_t i = 0; i < dt_out->ncols - 1; ++i) {
    RealColumn<double>* c = static_cast<RealColumn<double>*>(dt_out->columns[i]);
    const double* d_c = static_cast<const double*>(dt_out->columns[i]->data());

    r[i] =  (d_c[row_id] - c->min()) / (c->max() - c->min());
  }
}


double* Aggregator::generate_pmatrix(int32_t mcols, unsigned int seed) {
  std::default_random_engine generator;
  double* pmatrix;

  if (!seed) {
    std::random_device rd;
    seed = rd();
  }

  generator.seed(seed);
  std::normal_distribution<double> distribution(0.0, 1.0);

  pmatrix = new double[(dt_out->ncols - 1) * mcols];
  for (int32_t i = 0; i < (dt_out->ncols - 1) * mcols; ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


void Aggregator::project_row(double* r, int32_t row_id, double* pmatrix, int32_t mcols) {
  std::memset(r, 0, static_cast<size_t>(mcols) * sizeof(double));

  for (int32_t i = 0; i < (dt_out->ncols - 1) * mcols; ++i) {
    RealColumn<double>* c = static_cast<RealColumn<double>*> (dt_out->columns[i / mcols]);
    const double* d_c = static_cast<const double*>(dt_out->columns[i / mcols]->data());

    if (!ISNA<double>(d_c[row_id])) {
      r[i % mcols] +=  pmatrix[i] * (d_c[row_id] - c->min()) / (c->max() - c->min());
//TODO: handle missing values and do r[j] /= n normalization at the end
    }
  }
}
