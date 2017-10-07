#include "datatable.h"
#include "myassert.h"
#include "rowindex.h"
#include "utils.h"

/**
 * Merge datatables `dts` into the datatable `dt`, by columns. Datatable `dt`
 * will be modified in-place. If any datatable has less rows than the others,
 * it will be filled with NAs; with the exception of 1-row datatables which will
 * be expanded to the desired height by duplicating that row.
 */
DataTable* datatable_cbind(DataTable *dt, DataTable **dts, int ndts)
{
    int64_t ncols = dt->ncols;
    int64_t nrows = dt->nrows;
    for (int i = 0; i < ndts; i++) {
        ncols += dts[i]->ncols;
        if (nrows < dts[i]->nrows) nrows = dts[i]->nrows;
    }

    // First, materialize the "main" datatable if it is a view
    datatable_reify(dt);

    // Fix up the main datatable if it has too few rows
    if (dt->nrows < nrows) {
        for (int64_t i = 0; i < dt->ncols; i++) {
            dt->columns[i] = dt->columns[i]->realloc_and_fill(nrows);
        }
        dt->nrows = nrows;
    }

    // Append columns from `dts` into the "main" datatable
    dtrealloc(dt->columns, Column*, ncols + 1);
    dt->columns[ncols] = NULL;
    int64_t j = dt->ncols;
    for (int i = 0; i < ndts; i++) {
        int64_t ncolsi = dts[i]->ncols;
        int64_t nrowsi = dts[i]->nrows;
        if (dts[i]->rowindex) {
            RowIndex *ri = dts[i]->rowindex;
            for (int64_t ii = 0; ii < ncolsi; ii++) {
                Column *c = dts[i]->columns[ii]->extract(ri);
                if (nrowsi < nrows) c = c->realloc_and_fill(nrows);
                dt->columns[j++] = c;
            }
        } else {
            for (int64_t ii = 0; ii < ncolsi; ii++) {
                Column *c = dts[i]->columns[ii] == NULL ? NULL : dts[i]->columns[ii]->incref();
                if (nrowsi < nrows) c = c->realloc_and_fill(nrows);
                dt->columns[j++] = c;
            }
        }
    }
    assert(j == ncols);

    // Done.
    dt->ncols = ncols;
    return dt;
}
