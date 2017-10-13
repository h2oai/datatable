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
