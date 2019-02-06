//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "columnset.h"
#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "utils/alloc.h"
#include "utils/exceptions.h"


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
    size_t ncols,
    size_t nrows,
    DataTable* dt,
    int (*mapfn)(size_t row0, size_t row1, void** out)
) {
  size_t ncomputedcols = 0;
  for (size_t i = 0; i < ncols; i++) {
    ncomputedcols += (spec[i] < 0);
  }
  if (dt == nullptr && ncomputedcols < ncols) {
    throw RuntimeError() << "Missing DataTable";
  }
  if (ncomputedcols == 0) {
    throw ValueError() << "Num computed columns = 0";
  }

  void** out = dt::amalloc<void*>(ncomputedcols);
  Column** columns = dt::amalloc<Column*>(ncols + 1);
  columns[ncols] = nullptr;

  size_t j = 0;
  for (size_t i = 0; i < ncols; i++) {
    int64_t s = spec[i];
    if (s >= 0) {
      columns[i] = dt->columns[static_cast<size_t>(s)]->shallowcopy();
    } else {
      SType stype = static_cast<SType>(-s);
      columns[i] = Column::new_data_column(stype, nrows);
      out[j] = columns[i]->data_w();
      j++;
    }
  }

  if (ncomputedcols) {
    (*mapfn)(0, nrows, out);
  }

  return columns;
}
