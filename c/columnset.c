#include <stdlib.h>
#include <stdio.h>
#include "columnset.h"
#include "utils.h"


/**
 * Create an array of columns by taking a slice from columns of DataTable `dt`.
 */
Column**
columns_from_slice(DataTable *dt, int64_t start, int64_t count, int64_t step)
{
    if (dt == NULL) return NULL;
    Column **srccols = dt->columns;
    Column **columns = NULL;
    dtmalloc(columns, Column*, count + 1);
    columns[count] = NULL;

    for (int64_t i = 0, j = start; i < count; i++, j += step) {
        columns[i] = column_incref(srccols[j]);
    }
    return columns;
}



/**
 * Create a list of columns by extracting columns at the given indices from
 * datatable `dt`.
 */
Column**
columns_from_array(DataTable *dt, int64_t *indices, int64_t ncols)
{
    if (dt == NULL || indices == NULL) return NULL;
    Column **srccols = dt->columns;
    Column **columns = NULL;
    dtmalloc(columns, Column*, ncols + 1);
    columns[ncols] = NULL;

    for (int64_t i = 0; i < ncols; i++) {
        columns[i] = column_incref(srccols[indices[i]]);
    }
    return columns;
}



/**
 * Create a list of columns from "mixed" sources: some columns are taken from
 * the datatable `dt` directly, others are computed with function `mapfn`.
 *
 * Parameters
 * ----------
 * spec
 *     Array of length `ncols` specifying how each column in the output should
 *     be constructed. If a particular element of this array is non-negative,
 *     then it means that a column with such index should be extracted from
 *     the datatable `dt`. If the element is negative, then a new column with
 *     stype `-spec[i]` should be constructed.
 *
 * ncols
 *     The count of columns that should be returned; also the length of `spec`.
 *
 * dt
 *     Source datatable for columns that should be copied by reference. If all
 *     columns are computed, then this parameter may be NULL.
 *
 * mapfn
 *     Function that can be used to fill-in the "computed" data columns. This
 *     function's signature is `(row0, row1, out)`, and it will compute the data
 *     in a row slice `row0:row1` filling the data array `out`. The latter array
 *     will consist of `.data` buffers for all columns to be computed.
 */
Column** columns_from_mixed(
    int64_t *spec,
    int64_t ncols,
    int64_t nrows,
    DataTable *dt,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
) {
    int64_t ncomputedcols = 0;
    for (int64_t i = 0; i < ncols; i++) {
        ncomputedcols += (spec[i] < 0);
    }
    if (dt == NULL && ncomputedcols < ncols) dterrr0("Missing DataTable");
    if (ncomputedcols == 0) dterrr0("Num computed columns = 0");

    void **out = NULL;
    Column **columns = NULL;
    dtmalloc(out, void*, ncomputedcols);
    dtmalloc(columns, Column*, ncols + 1);
    columns[ncols] = NULL;

    int64_t j = 0;
    for (int64_t i = 0; i < ncols; i++) {
        if (spec[i] >= 0) {
            columns[i] = column_incref(dt->columns[spec[i]]);
        } else {
            SType stype = (SType)(-spec[i]);
            columns[i] = make_data_column(stype, (size_t) nrows);
            out[j] = columns[i]->data;
            j++;
        }
    }

    if (ncomputedcols) {
        (*mapfn)(0, nrows, out);
    }

    return columns;
}
