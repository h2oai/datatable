#include <stdlib.h>
#include <stdio.h>
#include "columnset.h"
#include "utils.h"


/**
 * Create a list of columns as a slice of columns from the DataTable `dt`.
 **/
Column**
columns_from_slice(DataTable *dt, int64_t start, int64_t count, int64_t step)
{
    if (dt == NULL) return NULL;
    Column **columns = NULL;
    Column **srccols = dt->columns;
    dtmalloc(columns, Column*, count + 1);
    columns[count] = NULL;

    for (int64_t i = 0, j = start; i < count; i++, j += step) {
        columns[i] = srccols[j];
        column_incref(columns[i]);
    }

    return columns;
}



/**
 * Create a list of columns by extracting columns `indices` from the
 * datatable `dt`.
 */
Column** columns_from_array(DataTable *dt, int64_t *indices, int64_t ncols)
{
    if (dt == NULL || indices == NULL) return NULL;
    Column **columns = NULL;
    Column **srccols = dt->columns;
    dtmalloc(columns, Column*, ncols + 1);
    columns[ncols] = NULL;

    for (int64_t i = 0; i < ncols; i++) {
        int64_t j = indices[i];
        columns[i] = (j >= 0)? srccols[j] : NULL;
        column_incref(columns[i]);
    }

    return columns;
}



/**
 *
 */
Column** columns_from_mixed(
    int64_t *spec,
    int64_t ncols,
    DataTable *dt,
    RowMapping *rowmapping,
    int (*mapfn)(int64_t row0, int64_t row1, void** out)
) {
    void **out = NULL;
    Column **columns = NULL;
    size_t nrows = (size_t) rowmapping->length;
    dtmalloc(out, void*, ncols);
    dtmalloc(columns, Column*, ncols + 1);
    columns[ncols] = NULL;

    int64_t j = 0;
    for (int64_t i = 0; i < ncols; i++) {
        if (spec[i] >= 0) {
            columns[i] = dt->columns[spec[i]];
            column_incref(columns[i]);
        } else {
            SType stype = (SType)(-spec[i]);
            size_t elemsize = stype_info[stype].elemsize;
            dtmalloc_v(out[j], elemsize * nrows);
            dtmalloc(columns[i], Column, 1);
            columns[i]->data = out[j];
            columns[i]->mtype = MT_DATA;
            columns[i]->stype = stype;
            columns[i]->meta = NULL;
            columns[i]->alloc_size = elemsize * nrows;
            columns[i]->refcount = 1;
            j++;
        }
    }

    (*mapfn)(0, (int64_t)nrows, out);

    return columns;
}
