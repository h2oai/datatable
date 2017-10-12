#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "column.h"
#include "datatable.h"
#include "myassert.h"
#include "rowindex.h"
#include "utils.h"



/**
 * Append to datatable `dt` a list of datatables `dts`. There are `ndts` items
 * in this list. The `cols` array of dimensions `ncols x ndts` specifies how
 * the columns should be matched.
 * In particular, the datatable `dt` will be expanded to have `ncols` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded datatable will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in datatable `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
DataTable*
DataTable::rbind(DataTable **dts, int **cols, int ndts, int64_t new_ncols)
{
    assert(new_ncols >= ncols);

    // If dt is a view datatable, then it must be converted to MT_DATA
    reify();

    dtrealloc(columns, Column*, new_ncols + 1);
    for (int64_t i = ncols; i < new_ncols; ++i) {
        columns[i] = NULL;
    }
    columns[new_ncols] = NULL;

    int64_t new_nrows = nrows;
    for (int i = 0; i < ndts; ++i) {
        new_nrows += dts[i]->nrows;
    }

    Column **cols0 = NULL;
    dtmalloc(cols0, Column*, ndts + 1);
    cols0[ndts] = NULL;

    for (int64_t i = 0; i < new_ncols; ++i) {
        Column *ret = NULL;
        Column *col0 = (i < ncols)
            ? columns[i]
            : Column::new_data_column(ST_VOID, nrows);
        for (int j = 0; j < ndts; ++j) {
            if (cols[i][j] < 0) {
                cols0[j] = Column::new_data_column(ST_VOID, dts[j]->nrows);
            } else {
                cols0[j] = dts[j]->columns[cols[i][j]]->extract();
            }
        }
        ret = col0->rbind(cols0);
        if (ret == NULL) return NULL;
        columns[i] = ret;
    }
    ncols = new_ncols;
    nrows = new_nrows;
    return this;
}
