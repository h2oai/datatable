#include "capi.h"
#include "datatable.h"
#include "rowindex.h"
extern "C" {


void* datatable_get_column_data(void* dt_, int64_t column)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  return dt->columns[column]->data();
}


void datatable_unpack_slicerowindex(void *dt_, int64_t *start, int64_t *step)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  RowIndex *ri = dt->rowindex;
  *start = ri->slice.start;
  *step = ri->slice.step;
}


void datatable_unpack_arrayrowindex(void *dt_, void **indices)
{
  DataTable *dt = static_cast<DataTable*>(dt_);
  RowIndex *ri = dt->rowindex;
  *indices = (void*) ri->ind32;
}


} // extern "C"
