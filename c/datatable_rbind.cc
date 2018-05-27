//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "datatable.h"
#include <vector>
#include "column.h"
#include "utils.h"
#include "utils/assert.h"



/**
 * Append to datatable `dt` a list of datatables `dts`. There are `ndts` items
 * in this list. The `cols` array of dimensions `ncols x ndts` specifies how
 * the columns should be matched.
 * In particular, the datatable `dt` will be expanded to have `ncols` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded datatable will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in datatable `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
void DataTable::rbind(DataTable** dts, int** cols, int ndts, int64_t new_ncols)
{
  xassert(new_ncols >= ncols);

  // If this is a view datatable, then it must be materialized.
  this->reify();

  dtrealloc_x(columns, Column*, new_ncols + 1);
  for (int64_t i = ncols; i < new_ncols; ++i) {
    columns[i] = new VoidColumn(nrows);
  }
  columns[new_ncols] = nullptr;

  int64_t new_nrows = nrows;
  for (int i = 0; i < ndts; ++i) {
    new_nrows += dts[i]->nrows;
  }

  size_t undts = static_cast<size_t>(ndts);
  std::vector<const Column*> cols_to_append(undts);
  for (int64_t i = 0; i < new_ncols; ++i) {
    for (size_t j = 0; j < undts; ++j) {
      int k = cols[i][j];
      Column* col = k < 0 ? new VoidColumn(dts[j]->nrows)
                          : dts[j]->columns[k]->shallowcopy();
      col->reify();
      cols_to_append[j] = col;
    }
    columns[i] = columns[i]->rbind(cols_to_append);
  }
  ncols = new_ncols;
  nrows = new_nrows;
}
