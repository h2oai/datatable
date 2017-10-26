//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#include "datatable.h"
#include <stdlib.h>
#include "myomp.h"
#include "py_utils.h"
#include "rowindex.h"
#include "types.h"
#include "datatable_check.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);



DataTable::DataTable(Column **cols)
  : nrows(0),
    ncols(0),
    rowindex(nullptr),
    columns(cols)
{
  if (cols == nullptr)
    throw Error("Column array cannot be null");
  if (cols[0] == nullptr) return;
  rowindex = cols[0]->rowindex();
  nrows = cols[0]->nrows;

  for (Column* col = cols[++ncols]; cols[ncols] != nullptr; ++ncols) {
    if (rowindex != col->rowindex())
      throw Error("Mismatched RowIndex in Column %" PRId64, ncols);
    if (nrows != col->nrows)
      throw Error("Mismatched length in Column %" PRId64 ": "
                  "found %" PRId64 ", expected %" PRId64,
                  ncols, col->nrows, nrows);
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
  columns[j] = NULL;
  // This may not be the same as `j` if there were repeating columns
  ncols = j;
  columns = static_cast<Column**>(realloc(columns, sizeof(Column*) * (size_t) (j + 1)));
  return this;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `DataTable_PyObject`s deallocator only.
 */
DataTable::~DataTable()
{
  if (rowindex) rowindex->release();
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
  if (!maskdt) throw Error("Mask cannot be NULL");
  if (ncols != maskdt->ncols || nrows != maskdt->nrows) {
    throw Error("Target datatable and mask have different shapes");
  }
  if (rowindex || maskdt->rowindex) {
    throw Error("Neither target DataTable nor the mask can be views");
  }

  for (int64_t i = 0; i < ncols; ++i){
    BoolColumn *maskcol = dynamic_cast<BoolColumn*>(maskdt->columns[i]);
    if (!maskcol) {
      throw Error("Column %lld in mask is not of a boolean type", i);
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
  if (rowindex == nullptr) return;
  for (int64_t i = 0; i < ncols; ++i) {
    columns[i]->reify();
  }
  rowindex->release();
  rowindex = nullptr;
}



size_t DataTable::memory_footprint()
{
  size_t sz = 0;
  sz += sizeof(*this);
  sz += (size_t)(ncols + 1) * sizeof(Column*);
  if (rowindex) {
    // If table is a view, then ignore sizes of each individual column.
    sz += rowindex->alloc_size();
  } else {
    for (int i = 0; i < ncols; ++i) {
      sz += columns[i]->memory_footprint();
    }
  }
  return sz;
}


/**
 * Macro to define the `stat_datatable()` methods. The `STAT` parameter
 * is the name of the statistic; exactly how it is named in the
 * `stat_datatable()` method.
 */
#define STAT_DATATABLE(STAT)                                                    \
  DataTable* DataTable:: STAT ## _datatable() const {                           \
    Column** out_cols = nullptr;                                                \
    dtmalloc(out_cols, Column*, (size_t) (ncols + 1));                          \
    for (int64_t i = 0; i < ncols; ++i)                                         \
    {                                                                           \
      out_cols[i] = columns[i]-> STAT ## _column();                             \
    }                                                                           \
    out_cols[ncols] = nullptr;                                                  \
    return new DataTable(out_cols);                                             \
  }

STAT_DATATABLE(mean)
STAT_DATATABLE(sd)
STAT_DATATABLE(countna)
STAT_DATATABLE(min)
STAT_DATATABLE(max)
STAT_DATATABLE(sum)

#undef STAT_DATATABLE

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
    if (rowindex != col->rowindex()) {
      icc << "Mismatch in `rowindex`: " << col_name << ".rowindex = "
          << col->rowindex() << ", while DataTable.rowindex=" << (rowindex)
          << end;
    }
    // Column check
    col->verify_integrity(icc, col_name);
  }

  if (columns[ncols] != NULL) {
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
