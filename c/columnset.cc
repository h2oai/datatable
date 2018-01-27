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
#include "columnset.h"
#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "utils/exceptions.h"


/**
 * Create an array of columns by taking a slice from columns of DataTable `dt`.
 */
Column** columns_from_slice(DataTable* dt, RowIndex* rowindex,
                            int64_t start, int64_t count, int64_t step)
{
  if (dt == nullptr) return nullptr;
  if (count < 0 || start < 0 || start >= dt->ncols ||
      start + step < 0 || start + step * (count - 1) >= dt->ncols) {
    throw ValueError() << "Invalid slice " << start << ":" << count << ":"
                       << step << " for a DataTable with " << dt->ncols
                       << " columns";
  }

  Column** srccols = dt->columns;
  Column** columns = nullptr;
  dtmalloc(columns, Column*, count + 1);
  columns[count] = nullptr;

  for (int64_t i = 0, j = start; i < count; i++, j += step) {
    columns[i] = srccols[j]->shallowcopy(rowindex);
  }
  return columns;
}



/**
 * Create a list of columns by extracting columns at the given indices from
 * datatable `dt`.
 */
Column** columns_from_array(DataTable* dt, RowIndex* rowindex,
                            int64_t* indices, int64_t ncols)
{
  if (dt == nullptr || indices == nullptr) return nullptr;
  Column** srccols = dt->columns;
  Column** columns = nullptr;
  dtmalloc(columns, Column*, ncols + 1);
  columns[ncols] = nullptr;

  for (int64_t i = 0; i < ncols; i++) {
    columns[i] = srccols[indices[i]]->shallowcopy(rowindex);
  }
  return columns;
}



/**
 * Create a list of columns from "mixed" sources: some columns are taken from
 * the datatable `dt` directly, others are computed with function `mapfn`.
 *
 * Parameters
 * ----------
 * spec
 *     Array of length `ncols` specifying how each column in the output should
 *     be constructed. If a particular element of this array is non-negative,
 *     then it means that a column with such index should be extracted from
 *     the datatable `dt`. If the element is negative, then a new column with
 *     stype `-spec[i]` should be constructed.
 *
 * ncols
 *     The count of columns that should be returned; also the length of `spec`.
 *
 * dt
 *     Source datatable for columns that should be copied by reference. If all
 *     columns are computed, then this parameter may be nullptr.
 *
 * mapfn
 *     Function that can be used to fill-in the "computed" data columns. This
 *     function's signature is `(row0, row1, out)`, and it will compute the data
 *     in a row slice `row0:row1` filling the data array `out`. The latter array
 *     will consist of `.data` buffers for all columns to be computed.
 */
Column** columns_from_mixed(
    int64_t* spec,
    int64_t ncols,
    int64_t nrows,
    DataTable* dt,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
) {
  int64_t ncomputedcols = 0;
  for (int64_t i = 0; i < ncols; i++) {
    ncomputedcols += (spec[i] < 0);
  }
  if (dt == nullptr && ncomputedcols < ncols) dterrr0("Missing DataTable");
  if (ncomputedcols == 0) dterrr0("Num computed columns = 0");

  void** out = nullptr;
  Column** columns = nullptr;
  dtmalloc(out, void*, ncomputedcols);
  dtmalloc(columns, Column*, ncols + 1);
  columns[ncols] = nullptr;

  int64_t j = 0;
  for (int64_t i = 0; i < ncols; i++) {
    if (spec[i] >= 0) {
      columns[i] = dt->columns[spec[i]]->shallowcopy();
    } else {
      SType stype = (SType)(-spec[i]);
      columns[i] = Column::new_data_column(stype, nrows);
      out[j] = columns[i]->data();
      j++;
    }
  }

  if (ncomputedcols) {
    (*mapfn)(0, nrows, out);
  }

  return columns;
}
