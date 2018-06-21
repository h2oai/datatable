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


DataTable* DataTable::aggregate(double epsilon, int64_t n_bins, int64_t nx_bins, int64_t ny_bins) {
  printf("call: void DataTable::aggregate()\n");

  DataTable** dts = nullptr;
  Column** cols = nullptr;

  dtmalloc(cols, Column*, 2);
  dtmalloc(dts, DataTable*, 1);

  cols[0] = Column::new_data_column(ST_INTEGER_I8, nrows);
  cols[1] = nullptr;
  dts[0] = new DataTable(cols);

  DataTable* ret = this->cbind(dts, 1);
  if (ret == nullptr) return nullptr;
  dtfree(dts);

  switch (ncols) {
    case 2 : aggregate1D(epsilon, n_bins); break;
    case 3 : aggregate2D(epsilon, nx_bins, ny_bins); break;
  	default: aggregateND();
  }

  return this;
}


DataTable* DataTable::aggregate1D(double epsilon, int64_t n_bins) {
  printf("call: void DataTable::aggregate1D()\n");

  LType ltype = stype_info[columns[0]->stype()].ltype;

  switch (ltype) {
	  case LT_INTEGER :
	  case LT_REAL 	  : aggregate1DContinuous(epsilon, n_bins); break;
  	  case LT_STRING  : aggregate1DCategorical(n_bins); break;
  	  default 		  : return nullptr;
  }

  return nullptr;
}


DataTable* DataTable::aggregate2D(double epsilon, int64_t nx_bins, int64_t ny_bins) {
  printf("call: void DataTable::aggregate2D()\n");

  LType ltype0 = stype_info[columns[0]->stype()].ltype;
  LType ltype1 = stype_info[columns[1]->stype()].ltype;

  switch (ltype0) {
	case LT_INTEGER :
	case LT_REAL 	: {  
		  	  	  	  	 switch (ltype1) {
		  	  	  	  	   case LT_INTEGER :
		  	  	  	  	   case LT_REAL    : aggregate2DContinuous(epsilon, nx_bins, ny_bins); break;
		  	  	  	  	   case LT_STRING  : aggregate2DMixed(epsilon, nx_bins, ny_bins); break;
		  	  	  	  	   default 		   : return nullptr;
		  	  	  	  	 }
	  	  	  	  	  }
	  	  	  	  	  break;

	case LT_STRING  : {
		  	  	  	     switch (ltype1) {
		  	  	  	  	   case LT_INTEGER :
		  	  	  	  	   case LT_REAL    : aggregate2DMixed(epsilon, nx_bins, ny_bins); break;
		  	  	  	  	   case LT_STRING  : aggregate2DCategorical(nx_bins, ny_bins); break;
		  	  	  	  	   default 		   : return nullptr;
		  	  	  	  	 }
	  	  	  	  	  }
	  	  	  	  	  break;

    default 		: return nullptr;
  }

  return nullptr;
}


void DataTable::aggregate1DContinuous(double epsilon, int64_t n_bins) {
  printf("call: void DataTable::aggregate1DContinuous()\n");

  RealColumn<double>* c0 = (RealColumn<double>*) columns[0]->cast(ST_REAL_F8);
  IntColumn<int64_t>* c1 = (IntColumn<int64_t>*) columns[1];
  int64_t idx_bin;

  for (int64_t i = 0; i < nrows; ++i) {
	 idx_bin = (int64_t) (n_bins * (1 - epsilon) * (c0->get_elem(i) - c0->min()) / (c0->max() - c0->min()));
	 c1->set_elem(i, idx_bin);
  }
}


void DataTable::aggregate2DContinuous(double epsilon, int64_t nx_bins, int64_t ny_bins) {
  printf("call: void DataTable::aggregate2DContinuous()\n");

  RealColumn<double>* c0 = (RealColumn<double>*) columns[0]->cast(ST_REAL_F8);
  RealColumn<double>* c1 = (RealColumn<double>*) columns[1]->cast(ST_REAL_F8);
  IntColumn<int64_t>* c2 = (IntColumn<int64_t>*) columns[2];
  int64_t id_bin, idx_bin, idy_bin;

  for (int64_t i = 0; i < nrows; ++i) {
	 idx_bin =  (int64_t) (nx_bins * (1 - epsilon) * (c0->get_elem(i) - c0->min()) / (c0->max() - c0->min()));
	 idy_bin = (int64_t) (ny_bins * (1 - epsilon) * (c1->get_elem(i) - c1->min()) / (c1->max() - c1->min()));
	 id_bin = nx_bins * idy_bin + idx_bin;
	 c2->set_elem(i, id_bin);
  }
}


void DataTable::aggregate1DCategorical(int64_t n_bins) {
	printf("call: void DataTable::aggregate1DCategorical()\n");
	// C++ implementation of Python's df(select=[first(f.C0),count(f.C0)],groupby="C0") to follow
	// count, first and groupby for one column are already implemented
}


void DataTable::aggregate2DCategorical(int64_t nx_bins, int64_t ny_bins) {
	printf("call: void DataTable::aggregate2DCategorical()\n");
	// C++ implementation of something like df(select=[first(f.C0),first(f.C1),count(f.C0,f.C1)],groupby="C0,C1") to follow
	// groupby can't do more than one column for the moment, see #1082
}


void DataTable::aggregate2DMixed(double epsilon, int64_t nx_bins, int64_t ny_bins) {
	printf("call: void DataTable::aggregate2DMixed()\n");
}


void DataTable::aggregateND() {
	printf("call: void DataTable::aggregateND()\n");
	// Implement Leland's algorithm for N-dimensional aggregation, see for more details
	// [1] https://www.cs.uic.edu/~wilkinson/Publications/outliers.pdf
	// [2] https://github.com/h2oai/vis-data-server/blob/master/library/src/main/java/com/h2o/data/Aggregator.java
}


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
