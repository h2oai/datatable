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
DataTable* DataTable::cbind(DataTable **dts, int ndts)
{
    int64_t t_ncols = ncols;
    int64_t t_nrows = nrows;
    for (int i = 0; i < ndts; ++i) {
        t_ncols += dts[i]->ncols;
        if (t_nrows < dts[i]->nrows) t_nrows = dts[i]->nrows;
    }

    // First, materialize the "main" datatable if it is a view
    reify();

    // Fix up the main datatable if it has too few rows
    if (nrows < t_nrows) {
        for (int64_t i = 0; i < ncols; ++i) {
            columns[i]->resize_and_fill(t_nrows);
        }
        nrows = t_nrows;
    }

    // Append columns from `dts` into the "main" datatable
    dtrealloc(columns, Column*, t_ncols + 1);
    columns[t_ncols] = NULL;
    int64_t j = ncols;
    for (int i = 0; i < ndts; ++i) {
        int64_t ncolsi = dts[i]->ncols;
        int64_t nrowsi = dts[i]->nrows;
        for (int64_t ii = 0; ii < ncolsi; ++ii) {
            Column *c = dts[i]->columns[ii]->extract();
            if (nrowsi < t_nrows) c->resize_and_fill(t_nrows);
            columns[j++] = c;
        }
    }
    assert(j == t_ncols);

    // Done.
    ncols = t_ncols;
    return this;
}
