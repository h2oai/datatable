//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_COLUMNSET_h
#define dt_COLUMNSET_h
#include "types.h"
#include "column.h"
#include "datatable.h"
#include "rowindex.h"


typedef int (columnset_mapfn)(int64_t row0, int64_t row1, void** out);

Column** columns_from_slice(
  DataTable* dt,
  const RowIndex& rowindex,
  int64_t start,
  int64_t count,
  int64_t step
);

Column** columns_from_array(
  DataTable *dt,
  const RowIndex& rowindex,
  int64_t *indices,
  int64_t ncols
);

Column** columns_from_mixed(
  int64_t *spec,
  int64_t ncols,
  int64_t nrows,
  DataTable *dt,
  columnset_mapfn *fn
);

#endif
