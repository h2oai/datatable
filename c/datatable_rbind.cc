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
 * Append to this Frame a list of other Frames `dts`. The `cols` array of
 * specifies how the columns should be matched.
 *
 * In particular, the Frame `dt` will be expanded to have `cols.size()` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded Frame will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in Frame `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
void DataTable::rbind(
    std::vector<DataTable*> dts, std::vector<std::vector<size_t>> cols)
{
  constexpr size_t INVALID_INDEX = size_t(-1);
  size_t new_ncols = cols.size();
  xassert(new_ncols >= ncols);

  // If this is a view Frame, then it must be materialized.
  this->reify();

  columns.resize(new_ncols, nullptr);
  for (size_t i = ncols; i < new_ncols; ++i) {
    columns[i] = new VoidColumn(nrows);
  }

  size_t new_nrows = nrows;
  for (auto dt : dts) {
    new_nrows += dt->nrows;
  }

  std::vector<const Column*> cols_to_append(dts.size(), nullptr);
  for (size_t i = 0; i < new_ncols; ++i) {
    for (size_t j = 0; j < dts.size(); ++j) {
      size_t k = cols[i][j];
      Column* col = k == INVALID_INDEX
                      ? new VoidColumn(dts[j]->nrows)
                      : dts[j]->columns[k]->shallowcopy();
      col->reify();
      cols_to_append[j] = col;
    }
    columns[i] = columns[i]->rbind(cols_to_append);
  }
  ncols = new_ncols;
  nrows = new_nrows;
}
