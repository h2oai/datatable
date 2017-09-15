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
datatable_rbind(DataTable *dt, DataTable **dts, int **cols, int ndts, int ncols)
{
    assert(ncols >= dt->ncols);
    dtrealloc(dt->columns, Column*, ncols + 1);
    dtrealloc(dt->stats, Stats*, ncols);
    for (int64_t i = dt->ncols; i < ncols; ++i) {
        dt->columns[i] = NULL;
        dt->stats[i] = NULL;
    }
    dt->columns[ncols] = NULL;

    int64_t nrows = dt->nrows;
    for (int i = 0; i < ndts; i++) {
        nrows += dts[i]->nrows;
    }

    // If dt is a view datatable, then it must be converted to MT_DATA
    if (dt->rowindex) {
        for (int i = 0; i < dt->ncols; i++) {
            Column *newcol = column_extract(dt->columns[i], dt->rowindex);
            if (newcol == NULL) return NULL;
            column_decref(dt->columns[i]);
            dt->columns[i] = newcol;
        }
        rowindex_decref(dt->rowindex);
        dt->rowindex = NULL;
    }

    Column **cols0 = NULL;
    dtmalloc(cols0, Column*, ndts + 1);
    cols0[ndts] = NULL;

    for (int i = 0; i < ncols; i++) {
        Column *ret = NULL;
        Column *col0 = (i < dt->ncols)
            ? dt->columns[i]
            : make_data_column(ST_VOID, (size_t) dt->nrows);
        for (int j = 0; j < ndts; j++) {
            if (cols[i][j] < 0) {
                cols0[j] = make_data_column(ST_VOID, (size_t) dts[j]->nrows);
            } else if (dts[j]->rowindex) {
                cols0[j] = column_extract(dts[j]->columns[cols[i][j]],
                                          dts[j]->rowindex);
            } else {
                cols0[j] = column_incref(dts[j]->columns[cols[i][j]]);
            }
            if (cols0[j] == NULL) return NULL;
        }
        ret = column_rbind(col0, cols0);
        if (ret == NULL) return NULL;
        dt->columns[i] = ret;
    }
    dt->ncols = ncols;
    dt->nrows = nrows;

    return dt;
}
