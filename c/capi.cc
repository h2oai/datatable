//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "capi.h"
#include "datatable.h"
#include "rowindex.h"
extern "C" {


void* datatable_get_column_data(void* dt_, size_t column)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  return dt->columns[column]->data_w();
}


void datatable_unpack_slicerowindex(void *dt_, int64_t *start, int64_t *step)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  RowIndex ri(dt->rowindex);
  *start = ri.slice_start();
  *step  = ri.slice_step();
}


void datatable_unpack_arrayrowindex(void *dt_, void **indices)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  RowIndex ri(dt->rowindex);
  *indices = const_cast<int32_t*>(ri.indices32());
}


} // extern "C"
