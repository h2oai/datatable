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
#ifndef dt_COLUMNSET_h
#define dt_COLUMNSET_h
#include "types.h"
#include "column.h"
#include "datatable.h"
#include "rowindex.h"


typedef int (columnset_mapfn)(int64_t row0, int64_t row1, void** out);

Column** columns_from_slice(
  DataTable* dt,
  RowIndex* rowindex,
  int64_t start,
  int64_t count,
  int64_t step
);

Column** columns_from_array(
  DataTable *dt,
  RowIndex* rowindex,
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
