#ifndef dt_COLUMNSET_H
#define dt_COLUMNSET_H
#include "types.h"
#include "column.h"
#include "datatable.h"


Column** columns_from_slice
(DataTable *dt, int64_t start, int64_t count, int64_t step);

Column** columns_from_array(DataTable *dt, int64_t *indices, int64_t ncols);

Column** columns_from_mapfn(
    SType *stypes, int64_t ncols, int64_t nrows,
    void (*mapfn)(int64_t row0, int64_t row1, void** out)
);

#endif
