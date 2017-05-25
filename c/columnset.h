#ifndef dt_COLUMNSET_H
#define dt_COLUMNSET_H
#include "types.h"
#include "column.h"
#include "datatable.h"
#include "rowmapping.h"


typedef int (columnset_mapfn)(int64_t row0, int64_t row1, void** out);

Column** columns_from_slice(
    DataTable *dt,
    int64_t start,
    int64_t count,
    int64_t step
);

Column** columns_from_array(DataTable *dt, int64_t *indices, int64_t ncols);

Column** columns_from_mapfn(
    SType *stypes, int64_t ncols, int64_t nrows, columnset_mapfn *fn
);


Column** columns_from_mixed(
    int64_t *spec,
    int64_t ncols,
    DataTable *dt,
    RowMapping *rowmapping,
    columnset_mapfn *fn
);

#endif
