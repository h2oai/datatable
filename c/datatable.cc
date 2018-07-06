//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "datatable.h"
#include <stdlib.h>
#include "utils/omp.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"
#include "datatable_check.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);



DataTable::DataTable(Column** cols)
  : nrows(0),
    ncols(0),
    columns(cols)
{
  if (cols == nullptr) {
    throw ValueError() << "Column array cannot be null";
  }
  if (cols[0] == nullptr) return;
  rowindex = RowIndex(cols[0]->rowindex());
  nrows = cols[0]->nrows;

  for (Column* col = cols[++ncols]; cols[ncols] != nullptr; ++ncols) {
    // TODO: restore, once Column also uses RowIndex
    // if (rowindex != col->rowindex()) {
    //   throw ValueError() << "Mismatched RowIndex in Column " << ncols;
    // }
    if (nrows != col->nrows) {
      throw ValueError() << "Mismatched length in Column " << ncols << ": "
                         << "found " << col->nrows << ", expected " << nrows;
    }
  }
}



DataTable* DataTable::delete_columns(int *cols_to_remove, int n)
{
  if (n == 0) return this;
  qsort(cols_to_remove, (size_t)n, sizeof(int), _compare_ints);
  int j = 0;
  int next_col_to_remove = cols_to_remove[0];
  int k = 0;
  for (int i = 0; i < ncols; ++i) {
    if (i == next_col_to_remove) {
      delete columns[i];
      do {
        ++k;
        next_col_to_remove = k < n ? cols_to_remove[k] : -1;
      } while (next_col_to_remove == i);
    } else {
      columns[j] = columns[i];
      ++j;
    }
  }
  columns[j] = nullptr;
  // This may not be the same as `j` if there were repeating columns
  ncols = j;
  columns = static_cast<Column**>(realloc(columns, sizeof(Column*) * (size_t) (j + 1)));
  return this;
}


DataTable* DataTable::aggregate(double epsilon, int32_t n_bins, int32_t nx_bins, int32_t ny_bins, int32_t max_dimensions, int32_t seed) {
  DataTable* dt = nullptr;
  Column** cols = nullptr;

  dtmalloc(cols, Column*, ncols + 2);

  for (int32_t i = 0; i < ncols; ++i) {
    LType ltype = stype_info[columns[i]->stype()].ltype;
	switch (ltype) {
	  case LT_BOOLEAN:
	  case LT_INTEGER:
	  case LT_REAL:    cols[i] = columns[i]->cast(ST_REAL_F8); break;
	  default:         cols[i] = columns[i]->shallowcopy();
	}
  }

  cols[ncols] = Column::new_data_column(ST_INTEGER_I4, nrows);
  cols[ncols + 1] = nullptr;
  dt = new DataTable(cols);

  switch (ncols) {
    case 1:  return aggregate_1d(dt, epsilon, n_bins);
    case 2:  return aggregate_2d(dt, epsilon, nx_bins, ny_bins);
    default: return aggregate_nd(dt, max_dimensions, seed);
  }
}


DataTable* DataTable::aggregate_1d(DataTable* dt, double epsilon, int32_t n_bins) {
  LType ltype = stype_info[dt->columns[0]->stype()].ltype;

  switch (ltype) {
    case LT_BOOLEAN:
    case LT_INTEGER:
    case LT_REAL:     aggregate_1d_continuous(dt, epsilon, n_bins); break;
    case LT_STRING:   aggregate_1d_categorical(dt, n_bins); break;
    default:          return nullptr;
  }

  return dt;
}


DataTable* DataTable::aggregate_2d(DataTable* dt, double epsilon, int32_t nx_bins, int32_t ny_bins) {
  LType ltype0 = stype_info[dt->columns[0]->stype()].ltype;
  LType ltype1 = stype_info[dt->columns[1]->stype()].ltype;

  switch (ltype0) {
    case LT_BOOLEAN:
    case LT_INTEGER:
    case LT_REAL:    {
                        switch (ltype1) {
                          case LT_BOOLEAN:
                          case LT_INTEGER:
                          case LT_REAL:    aggregate_2d_continuous(dt, epsilon, nx_bins, ny_bins); break;
                          case LT_STRING:  aggregate_2d_mixed(dt, 0, epsilon, nx_bins, ny_bins); break;
                          default:         return nullptr;
                        }
                      }
                      break;

    case LT_STRING:  {
                        switch (ltype1) {
                          case LT_BOOLEAN:
                          case LT_INTEGER:
                          case LT_REAL:    aggregate_2d_mixed(dt, 1, epsilon, nx_bins, ny_bins); break;
                          case LT_STRING:  aggregate_2d_categorical(dt, nx_bins, ny_bins); break;
                          default:         return nullptr;
                        }
                      }
                      break;

    default:          return nullptr;
  }

  return dt;
}


void DataTable::aggregate_1d_continuous(DataTable* dt, double epsilon, int32_t n_bins) {
  RealColumn<double>* c0 = (RealColumn<double>*) dt->columns[0];
  IntColumn<int32_t>* c1 = (IntColumn<int32_t>*) dt->columns[1];
  int32_t idx_bin;
  double norm_factor = n_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (int32_t i = 0; i < nrows; ++i) {
	idx_bin = (int32_t) (norm_factor * (c0->get_elem(i) - c0->min()));
    c1->set_elem(i, idx_bin);
  }
}


void DataTable::aggregate_2d_continuous(DataTable* dt, double epsilon, int32_t nx_bins, int32_t ny_bins) {
  RealColumn<double>* c0 = (RealColumn<double>*) dt->columns[0];
  RealColumn<double>* c1 = (RealColumn<double>*) dt->columns[1];
  IntColumn<int32_t>* c2 = (IntColumn<int32_t>*) dt->columns[2];
  int32_t id_bin, idx_bin, idy_bin;
  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());
  double normy_factor = ny_bins * (1 - epsilon) / (c1->max() - c1->min());

  for (int32_t i = 0; i < nrows; ++i) {
    idx_bin =  (int32_t) (normx_factor * (c0->get_elem(i) - c0->min())) ;
    idy_bin = (int32_t) (normy_factor * (c1->get_elem(i) - c1->min()));
    id_bin = nx_bins * idy_bin + idx_bin;
    c2->set_elem(i, id_bin);
  }
}


void DataTable::aggregate_1d_categorical(DataTable* dt, int32_t n_bins /* not sure how to use n_bins for the moment */) {
  IntColumn<int32_t>* c1 = (IntColumn<int32_t>*) dt->columns[1];
  arr32_t cols(1);
  Groupby grpby;

  cols[0] = 0;
  RowIndex ri_group = dt->sortby(cols, &grpby);
  const RowIndex ri_ungroup = grpby.ungroup_rowindex();
  const int32_t* i_group = ri_group.indices32();
  const int32_t* i_ungroup = ri_ungroup.indices32();

  for (int32_t i = 0; i < nrows; ++i) {
    c1->set_elem(i_group[i], i_ungroup[i]);
  }
// TODO: store ri somehow to be reused for groupping with dt->replace_rowindex(ri);
}


void DataTable::aggregate_2d_categorical(DataTable* dt, int32_t nx_bins, int32_t ny_bins /* not sure how to use ny_bins for the moment */) {
// TODO: C++ implementation of something like df(select=[first(f.C0),first(f.C1),count(f.C0,f.C1)],groupby="C0,C1") to follow
// groupby can't do more than one column for the moment, see #1082
}


void DataTable::aggregate_2d_mixed(DataTable* dt, bool cont_index, double epsilon, int32_t nx_bins, int32_t ny_bins) {
  RealColumn<double>* c0 = (RealColumn<double>*) dt->columns[cont_index];
  IntColumn<int32_t>* c2 = (IntColumn<int32_t>*) dt->columns[2];
  int32_t id_bin, idx_bin;
  arr32_t cols(1);
  Groupby grpby;

  cols[0] = !cont_index;
  RowIndex ri_group = dt->sortby(cols, &grpby);
  const RowIndex ri_ungroup = grpby.ungroup_rowindex();
  const int32_t* i_group = ri_group.indices32();
  const int32_t* i_ungroup = ri_ungroup.indices32();

  double normx_factor = nx_bins * (1 - epsilon) / (c0->max() - c0->min());

  for (int32_t i = 0; i < nrows; ++i) {
    idx_bin =  (int32_t) (normx_factor * (c0->get_elem(i_group[i]) - c0->min()));
    id_bin = nx_bins * i_ungroup[i] + idx_bin;
    c2->set_elem(i_group[i], id_bin);
  }
}


void DataTable::adjust_radius(DataTable* dt, int32_t mcols, double& radius) {
  double diff = 0;
  for (int i = 0; i < ncols; ++i) {
    RealColumn<double>* ci = (RealColumn<double>*) dt->columns[i];
    diff += (ci->max() - ci->min()) * (ci->max() - ci->min());
  }

  diff /= ncols;
  radius = .05 * log(mcols);
  if (diff > 10000.0) radius *= .4;
}

// Leland's algorithm for N-dimensional aggregation, see [1-2] for more details
// [1] https://www.cs.uic.edu/~wilkinson/Publications/outliers.pdf
// [2] https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/h2o/data/Aggregator.java
DataTable* DataTable::aggregate_nd(DataTable* dt, int32_t mcols, int32_t seed) {
  size_t exemplar_id = 0;
  int64_t ndims = min(mcols, ncols);
  double* exemplar = new double[ndims];
  double* member = new double[ndims];
  double* pmatrix = nullptr;
//  double** pmatrix = nullptr;
  std::vector<double*> exemplars;
  double delta, radius = .025 * ncols, distance = 0.0;
  IntColumn<int32_t>* c = (IntColumn<int32_t>*) dt->columns[ncols];

  if (ncols > mcols) {
    adjust_radius(dt, mcols, radius);
    pmatrix = generate_pmatrix(mcols, seed);
    project_row(dt, exemplar, 0, pmatrix, mcols);
  } else normalize_row(dt, exemplar, 0);

  delta = radius * radius;
  exemplars.push_back(exemplar);
  c->set_elem(0, 0);

  for (int32_t i = 1; i < nrows; ++i) {
    double min_distance = std::numeric_limits<double>::max();
    if (ncols > mcols) project_row(dt, member, i, pmatrix, mcols);
    else normalize_row(dt, member, i);

    for (size_t j = 0; j < exemplars.size(); ++j) {
      distance = calculate_distance(member, exemplars[j], ndims, delta);

      if (distance < min_distance) {
        min_distance = distance;
        exemplar_id = j;
        if (min_distance < delta) {
          break;
        }
      }
    }

    if (min_distance < delta) {
      c->set_elem(i, (int32_t) exemplar_id);
    } else {
      c->set_elem(i, (int32_t) exemplars.size());
      exemplars.push_back(member);
      member = new double[ndims];
    }
  }

  delete[] member;
  for (size_t i= 0; i < exemplars.size(); ++i) {
    delete[] exemplars[i];
  }

  delete[] pmatrix;
//  if (pmatrix != nullptr) {
//    for (int32_t i = 0; i < ncols; ++i) {
//      delete[] pmatrix[i];
//    }
//  }

  return dt;
}

double DataTable::calculate_distance(double* e1, double* e2, int64_t ndims, double delta) {
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


void DataTable::normalize_row(DataTable* dt, double* r, int32_t row_id) {
  for (int32_t i = 0; i < ncols; ++i) {
    RealColumn<double>* c = (RealColumn<double>*) dt->columns[i];
    r[i] =  (c->get_elem(row_id) - c->min()) / (c->max() - c->min());
  }
}


double* DataTable::generate_pmatrix(int32_t mcols, int32_t seed) {
  std::default_random_engine generator;
  double* pmatrix;

  if (!seed) {
    std::random_device rd;
    seed = rd();
  }

  generator.seed(seed);
  std::normal_distribution<double> distribution(0.0, 1.0);

  pmatrix = new double[ncols*mcols];
  for (int32_t i = 0; i < ncols*mcols; ++i) {
    pmatrix[i] = distribution(generator);
  }

  return pmatrix;
}


void DataTable::project_row(DataTable* dt, double* r, int32_t row_id, double* pmatrix, int32_t mcols) {
  std::memset(r, 0, ((size_t) mcols) * sizeof(double));

  for (int32_t i = 0; i < ncols*mcols; ++i) {
    RealColumn<double>* c = (RealColumn<double>*) dt->columns[i / mcols];
    if (!ISNA<double>(c->get_elem(row_id))) {
      r[i % mcols] +=  pmatrix[i] * (c->get_elem(row_id) - c->min()) / (c->max() - c->min());
// TODO: handle missing values and do r[j] /= n normalization at the end
    }
  }
}


//double** DataTable::generate_pmatrix(int32_t mcols, int32_t seed) {
//  std::default_random_engine generator;
//  double** pmatrix;
//
//  if (!seed) {
//    std::random_device rd;
//    seed = rd();
//  }
//
//  generator.seed(seed);
//  std::normal_distribution<double> distribution(0.0, 1.0);
//
//  pmatrix = new double*[ncols];
//
//  for (int32_t i = 0; i < ncols; ++i) {
//    pmatrix[i] = new double[mcols];
//    for (int32_t j = 0; j < mcols; ++j) {
//      pmatrix[i][j] = distribution(generator);
//    }
//  }
//
//  return pmatrix;
//}
//
//
//void DataTable::project_row(DataTable* dt, double* r, int32_t row_id, double** pmatrix, int32_t mcols) {
//  std::memset(r, 0, ((size_t) mcols) * sizeof(double));
//
//  for (int32_t i = 0; i < ncols; ++i) {
//    RealColumn<double>* c = (RealColumn<double>*) dt->columns[i];
//    for (int32_t j = 0; j < mcols; ++j) {
//      if (!ISNA<double>(c->get_elem(row_id))) {
//        r[j] +=  pmatrix[i][j] * (c->get_elem(row_id) - c->min()) / (c->max() - c->min());
//// TODO: handle missing values and do r[j] /= n normalization at the end
//      }
//    }
//  }
//}


void DataTable::resize_rows(int64_t new_nrows) {
  if (rowindex) {
    if (new_nrows < nrows) {
      rowindex.shrink(new_nrows, ncols);
      replace_rowindex(rowindex);
      return;
    }
    if (new_nrows > nrows) {
      reify();
      // fall-through
    }
  }
  if (new_nrows != nrows) {
    for (int64_t i = 0; i < ncols; ++i) {
      columns[i]->resize_and_fill(new_nrows);
    }
    nrows = new_nrows;
  }
}



void DataTable::replace_rowindex(const RowIndex& newri) {
  if (newri.isabsent() && rowindex.isabsent()) return;
  rowindex = newri;
  nrows = rowindex.length();
  for (int64_t i = 0; i < ncols; ++i) {
    columns[i]->replace_rowindex(rowindex);
  }
}


void DataTable::replace_groupby(const Groupby& newgb) {
  int32_t last_offset = newgb.offsets_r()[newgb.ngroups()];
  if (last_offset != nrows) {
    throw ValueError() << "Cannot apply Groupby of " << last_offset << " rows "
      "to a Frame with " << nrows << " rows";
  }
  groupby = newgb;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `pydatatable::obj`s deallocator only.
 */
DataTable::~DataTable()
{
  for (int64_t i = 0; i < ncols; ++i) {
    delete columns[i];
  }
  delete columns;
}


// Comparator function to sort integers using `qsort`
static inline int _compare_ints(const void *a, const void *b) {
  const int x = *(const int*)a;
  const int y = *(const int*)b;
  return (x > y) - (x < y);
}


/**
 * Modify datatable replacing values that are given by the mask with NAs.
 * The target datatable must have the same shape as the mask, and neither can
 * be a view.
 */
void DataTable::apply_na_mask(DataTable* maskdt)
{
  if (!maskdt) {
    throw ValueError() << "Mask cannot be NULL";
  }
  if (ncols != maskdt->ncols || nrows != maskdt->nrows) {
    throw ValueError() << "Target datatable and mask have different shapes";
  }
  if (!(rowindex.isabsent() || maskdt->rowindex.isabsent())) {
    throw ValueError() << "Neither target DataTable nor the mask can be views";
  }

  for (int64_t i = 0; i < ncols; ++i){
    BoolColumn *maskcol = dynamic_cast<BoolColumn*>(maskdt->columns[i]);
    if (!maskcol) {
      throw ValueError() << "Column " << i
                         << " in mask is not of a boolean type";
    }
    Column *col = columns[i];
    col->apply_na_mask(maskcol);
  }
}

/**
 * Convert a DataTable view into an actual DataTable. This is done in-place.
 * The resulting DataTable should have a NULL RowIndex and Stats array.
 * Do nothing if the DataTable is not a view.
 */
void DataTable::reify() {
  if (rowindex.isabsent()) return;
  for (int64_t i = 0; i < ncols; ++i) {
    columns[i]->reify();
  }
  rowindex.clear();
}



size_t DataTable::memory_footprint()
{
  size_t sz = 0;
  sz += sizeof(*this);
  sz += (size_t)(ncols + 1) * sizeof(Column*);
  if (rowindex.isabsent()) {
    for (int i = 0; i < ncols; ++i) {
      sz += columns[i]->memory_footprint();
    }
  } else {
    // If table is a view, then ignore sizes of each individual column.
    sz += rowindex.memory_footprint();
  }
  return sz;
}



//------------------------------------------------------------------------------
// Compute stats
//------------------------------------------------------------------------------

DataTable* DataTable::_statdt(colmakerfn f) const {
  Column** out_cols = new Column*[ncols + 1];
  for (int64_t i = 0; i < ncols; ++i) {
    out_cols[i] = (columns[i]->*f)();
  }
  out_cols[ncols] = nullptr;
  return new DataTable(out_cols);
}

DataTable* DataTable::countna_datatable() const { return _statdt(&Column::countna_column); }
DataTable* DataTable::nunique_datatable() const { return _statdt(&Column::nunique_column); }
DataTable* DataTable::nmodal_datatable() const  { return _statdt(&Column::nmodal_column); }
DataTable* DataTable::mean_datatable() const    { return _statdt(&Column::mean_column); }
DataTable* DataTable::sd_datatable() const      { return _statdt(&Column::sd_column); }
DataTable* DataTable::min_datatable() const     { return _statdt(&Column::min_column); }
DataTable* DataTable::max_datatable() const     { return _statdt(&Column::max_column); }
DataTable* DataTable::mode_datatable() const    { return _statdt(&Column::mode_column); }
DataTable* DataTable::sum_datatable() const     { return _statdt(&Column::sum_column); }




/**
 * Verify that all internal constraints in the DataTable hold, and that there
 * are no any inappropriate values/elements.
 */
bool DataTable::verify_integrity(IntegrityCheckContext& icc) const
{
  int nerrs = icc.n_errors();
  auto end = icc.end();

  // Check that the number of rows in nonnegative
  if (nrows < 0) {
    icc << "DataTable has a negative value for `nrows`: " << nrows << end;
  }
  // Check that the number of columns is nonnegative
  if (ncols < 0) {
    icc << "DataTable has a negative value for `ncols`: " << ncols << end;
  }

  // Check the number of columns; the number of allocated columns should be
  // equal to `ncols + 1` (with extra column being NULL). Sometimes the
  // allocation size can be greater than the required number of columns,
  // because `malloc()` may allocate more than requested.
  size_t n_cols_allocd = array_size(columns, sizeof(Column*));
  if (!columns || !n_cols_allocd) {
    icc << "DataTable.columns array of is not allocated" << end;
  }
  else if (ncols + 1 > static_cast<int64_t>(n_cols_allocd)) {
    icc << "DataTable.columns array size is " << n_cols_allocd
        << " whereas " << ncols + 1 << " columsn are expected." << end;
  }
  if (icc.has_errors(nerrs)) return false;

  /**
   * Check the structure and contents of the column array.
   *
   * DataTable's RowIndex and nrows are supposed to reflect the RowIndex and
   * nrows of each column, so we will just check that the datatable's values
   * are equal to those of each column.
   **/
  for (int64_t i = 0; i < ncols; ++i) {
    std::string col_name = "Column "_s + std::to_string(i);
    Column* col = columns[i];
    if (col == nullptr) {
      icc << col_name << " of DataTable is null" << end;
      continue;
    }
    // Make sure the column and the datatable have the same value for `nrows`
    if (nrows != col->nrows) {
      icc << "Mismatch in `nrows`: " << col_name << ".nrows = " << col->nrows
          << ", while the DataTable has nrows=" << nrows << end;
    }
    // Make sure the column and the datatable point to the same rowindex object
    // TODO: restore
    // if (rowindex != col->rowindex()) {
    //   icc << "Mismatch in `rowindex`: " << col_name << ".rowindex = "
    //       << col->rowindex() << ", while DataTable.rowindex=" << (rowindex)
    //       << end;
    // }
    // Column check
    col->verify_integrity(icc, col_name);
  }

  if (columns[ncols] != nullptr) {
    // Memory was allocated for `ncols+1` columns, but the last element
    // was not set to NULL.
    // Note that if `cols` array was under-allocated and `malloc_size`
    // not available on this platform, then this might segfault... This is
    // unavoidable since if we skip the check and do `cols[ncols]` later on
    // then we will segfault anyways.
    icc << "Last entry in the `columns` array of DataTable is not null" << end;
  }
  return !icc.has_errors(nerrs);
}
