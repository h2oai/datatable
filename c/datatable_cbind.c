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
    if (dt->rowindex) {
        for (int64_t i = 0; i < dt->ncols; i++) {
            Column *newcol = column_extract(dt->columns[i], dt->rowindex);
            column_decref(dt->columns[i]);
            dt->columns[i] = newcol;
        }
        rowindex_dealloc(dt->rowindex);
        dt->rowindex = NULL;
    }

    // Fix up the main datatable if it has too few rows
    if (dt->nrows < nrows) {
        for (int64_t i = 0; i < dt->ncols; i++) {
            dt->columns[i] = column_realloc_and_fill(dt->columns[i], nrows);
        }
        dt->nrows = nrows;
    }

    // Append columns from `dts` into the "main" datatable
    dtrealloc(dt->columns, Column*, ncols + 1);
    dtrealloc(dt->stats, Stats*, ncols);
    dt->columns[ncols] = NULL;
    int64_t j = dt->ncols;
    for (int i = 0; i < ndts; i++) {
        int64_t ncolsi = dts[i]->ncols;
        int64_t nrowsi = dts[i]->nrows;
        if (dts[i]->rowindex) {
            RowIndex *ri = dts[i]->rowindex;
            for (int64_t ii = 0; ii < ncolsi; ii++) {
                Column *c = column_extract(dts[i]->columns[ii], ri);
                if (nrowsi < nrows) c = column_realloc_and_fill(c, nrows);
                dt->stats[j] = NULL;
                dt->columns[j++] = c;
            }
        } else {
            for (int64_t ii = 0; ii < ncolsi; ii++) {
                Column *c = column_incref(dts[i]->columns[ii]);
                if (nrowsi < nrows) c = column_realloc_and_fill(c, nrows);
                dt->stats[j] = NULL;
                dt->columns[j++] = c;
            }
        }
    }
    assert(j == ncols);

    // Done.
    dt->ncols = ncols;
    return dt;
}
