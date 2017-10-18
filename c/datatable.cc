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
    throw new Error("Column array cannot be null");
  if (cols[0] == nullptr) return;
  rowindex = cols[0]->rowindex();
  nrows = cols[0]->nrows;

  for (Column* col = cols[++ncols]; cols[ncols] != nullptr; ++ncols) {
    if (rowindex != col->rowindex())
      throw new Error("Mismatched RowIndex in Column %" PRId64, ncols);
    if (nrows != col->nrows)
      throw new Error("Mismatched length in Column %" PRId64 ": "
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
  if (!maskdt) throw new Error("Mask cannot be NULL");
  if (ncols != maskdt->ncols || nrows != maskdt->nrows) {
    throw new Error("Target datatable and mask have different shapes");
  }
  if (rowindex || maskdt->rowindex) {
    throw new Error("Neither target DataTable nor the mask can be views");
  }

  for (int64_t i = 0; i < ncols; ++i){
    BoolColumn *maskcol = dynamic_cast<BoolColumn*>(maskdt->columns[i]);
    if (!maskcol) {
      throw new Error("Column %lld in mask is not of a boolean type", i);
    }
    Column *col = columns[i];
    col->stats->reset();
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
    Column *newcol = columns[i]->extract();
    delete columns[i];
    columns[i] = newcol;
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
 * Verify that the DataTable does not have any inappropriate values/elements
 *
 * @param errors
 *     Reference to an empty vector of `chars` that will be filled with
 *     diagnostic messages containing information about any problems found.
 *
 * @param max_errors
 *     The maximum amount of messages to be printed in `errors`. A negative
 *     argument will be treated as a zero. The return value is not
 *     affected by this parameter.
 *
 * @param name
 *     The name to be used when referring to this DataTable instance in error
 *     messages.
 *
 * @return
 *     The number of errors found.
 */
int DataTable::verify_integrity(
    std::vector<char> *errors, int max_errors, const char *name) const
{
  int nerrors = 0;
  if (errors == nullptr) return nerrors; // TODO: do something different?

  /**
   * Sanity checks. Nothing else is checked if these don't pass.
   */
  // Check that number of rows in nonnegative
  if (nrows < 0) {
    ERR("%s reports a negative value for `nrows`: %lld\n", name, nrows);
    return nerrors;
  }
  // Check that the number of columns is nonnegative
  if (ncols < 0) {
    ERR("%s reports a negative value for `ncols`: %lld\n", name, ncols);
    return nerrors;
  }
  // Check the number of columns; the number of allocated columns should be
  // equal to `ncols + 1` (with extra column being NULL). Sometimes the
  // allocation size can be greater than the required number of columns,
  // because `malloc()` may allocate more than requested.
  size_t n_cols_allocd = array_size(columns, sizeof(Column*));
  if (!columns || !n_cols_allocd) {
    ERR("`columns` array of %s is not allocated\n", name);
    return nerrors;
  }
  if (ncols + 1 > (int64_t) n_cols_allocd) {
    ERR("`columns` array size of %s is not larger than `ncols`: "
        "%lld vs. %lld\n",
        name, (int64_t) n_cols_allocd, ncols);
    return nerrors;
  }

  /**
   * Check the structure and contents of the column array.
   *
   * DataTable's RowIndex and nrows are supposed to reflect the RowIndex and
   * nrows of each column, so we will just check that the datatable's values
   * are equal to those of each column.
   **/
  for (int64_t i = 0; i < ncols; ++i) {
    Column* col = columns[i];
    char col_name[28];
    snprintf(col_name, 28, "Column %lld", i);
    if (col == nullptr) {
      ERR("%s of %s is null\n", col_name, name);
      continue;
    }
    // Column check
    int col_nerrors = col->verify_integrity(errors, max_errors - nerrors, col_name);
    if (col_nerrors > 0) {
      nerrors += col_nerrors;
      continue;
    }
    // Make sure column and datatable have the same value for `nrows`
    if (nrows != col->nrows) {
      ERR("Mismatch in `nrows`: %s reports %lld, %s reports %lld\n",
          col_name, col->nrows, name, nrows);
    }
    // Make sure column and datatable point to the same rowindex instance
    if (rowindex != col->rowindex()) {
      ERR("Mismatch in `rowindex` instance: %s points to %p, %s points to %p\n",
          col_name, col->rowindex(), name, rowindex);
    }
  }

  if (columns[ncols] != NULL) {
    // Memory was allocated for `ncols+1` columns, but the last element
    // was not set to NULL.
    // Note that if `cols` array was under-allocated and `malloc_size`
    // not available on this platform, then this might segfault... This is
    // unavoidable since if we skip the check and do `cols[ncols]` later on
    // then we will segfault anyways.
    ERR("Last entry in the `columns` array of %s is not null\n", name);
  }
  return nerrors;
}
