#include <stdlib.h>
#include "columnset.h"
#include "utils.h"


/**
 * Create a list of columns as a columnar slice of the DataTable `dt`.
 **/
Column**
columns_from_slice(DataTable *dt, int64_t start, int64_t count, int64_t step)
{
    if (dt == NULL) return NULL;
    ViewColumn **columns = NULL;
    Column **srccols = dt->columns;

    dtmalloc(columns, ViewColumn*, count + 1);
    int64_t j = start;
    for (int64_t i = 0; i < count; i++) {
        dtmalloc(columns[i], ViewColumn, 1);
        columns[i]->srcindex = j;
        columns[i]->mtype = MT_VIEW;
        columns[i]->stype = srccols[j]->stype;
        j += step;
    }
    columns[count] = NULL;
    return (Column**) columns;

  fail:
    dtfree(columns);
    return NULL;
}



Column** columns_from_array(DataTable *dt, int64_t *indices, int64_t ncols)
{
    if (dt == NULL || indices == NULL) return NULL;
    ViewColumn **columns = NULL;
    Column **srccols = dt->columns;

    dtmalloc(columns, ViewColumn*, ncols + 1);
    for (int64_t i = 0; i < ncols; i++) {
        int64_t j = indices[i];
        if (j < 0) {
            columns[i] = NULL;
        } else {
            dtmalloc(columns[i], ViewColumn, 1);
            columns[i]->srcindex = j;
            columns[i]->mtype = MT_VIEW;
            columns[i]->stype = srccols[j]->stype;
        }
    }
    columns[ncols] = NULL;
    return (Column**) columns;

  fail:
    dtfree(columns);
    return NULL;
}



Column** columns_from_mapfn(
    SType *stypes, int64_t ncols, int64_t nrows,
    void (*mapfn)(int64_t row0, int64_t row1, void** out)
) {
    void **out = NULL;
    Column **columns = NULL;
    dtmalloc(out, void*, ncols);
    dtmalloc(columns, Column*, ncols + 1);
    int64_t j = 0;
    for (int64_t i = 0; i < ncols; i++) {
        if (stypes[i] == 0) {
            columns[i] = NULL;
        } else {
            size_t elemsize = stype_info[stypes[i]].elemsize;
            out[j] = malloc(elemsize * (size_t)nrows);
            dtmalloc(columns[i], Column, 1);
            columns[i]->data = out[j];
            columns[i]->mtype = MT_DATA;
            columns[i]->stype = stypes[i];
            columns[i]->meta = NULL;
            columns[i]->alloc_size = elemsize * (size_t)nrows;
            j++;
        }
    }
    columns[ncols] = NULL;

    (*mapfn)(0, nrows, out);
    return columns;

  fail:
    dtfree(out);
    return NULL;
}

