#include "datatable.h"
#include <vector>
#include "myassert.h"



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
DataTable* DataTable::rbind(DataTable **dts, int **cols, int ndts,
                            int64_t new_ncols)
{
    assert(new_ncols >= ncols);

    // If this is a view datatable, then it must be materialized.
    this->reify();

    dtrealloc(columns, Column*, new_ncols + 1);
    for (int64_t i = ncols; i < new_ncols; ++i) {
        columns[i] = new VoidColumn(nrows);
    }
    columns[new_ncols] = NULL;

    int64_t new_nrows = nrows;
    for (int i = 0; i < ndts; ++i) {
        new_nrows += dts[i]->nrows;
    }

    std::vector<const Column*> cols_to_append(ndts);
    for (int64_t i = 0; i < new_ncols; ++i) {
        for (int j = 0; j < ndts; ++j) {
            int k = cols[i][j];
            cols_to_append[j] = k < 0 ? new VoidColumn(dts[j]->nrows)
                                      : dts[j]->columns[k]->extract();
        }
        columns[i] = columns[i]->rbind(cols_to_append);
    }
    ncols = new_ncols;
    nrows = new_nrows;
    return this;
}
