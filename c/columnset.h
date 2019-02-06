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


typedef int (columnset_mapfn)(size_t row0, size_t row1, void** out);

Column** columns_from_mixed(
  int64_t *spec,
  size_t ncols,
  size_t nrows,
  DataTable *dt,
  columnset_mapfn *fn
);

#endif
