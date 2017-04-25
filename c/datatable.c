#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "datatable.h"
#include "rowmapping.h"
#include "colmapping.h"

// Forward declarations
static void* _extract_column(DataTable *dt, int64_t i, RowMapping *rowmapping);


/**
 * Main "driver" function for the DataTable. Corresponds to DataTable.__call__
 * in Python.
 */
DataTable* dt_DataTable_call(
    DataTable *self, RowMapping *rowmapping, ColMapping *colmapping)
{
    int64_t ncols = colmapping->length;
    int64_t nrows = rowmapping->length;

    // Computed on-demand only if we detect that it is needed
    RowMapping *merged_rowindex = NULL;

    Column *columns = calloc(sizeof(Column), (size_t)ncols);
    if (columns == NULL) return NULL;

    for (int64_t i = 0; i < ncols; ++i) {
        columns[i].stype = colmapping->stypes[i];
        int64_t j = colmapping->indices[i];
        if (self->columns[j].data == NULL) {
            if (merged_rowindex == NULL) {
                merged_rowindex = RowMapping_merge(self->rowmapping, rowmapping);
            }
            columns[i].data = NULL;
            columns[i].srcindex = self->columns[j].srcindex;
        }
        else if (self->source == NULL) {
            columns[i].data = NULL;
            columns[i].srcindex = j;
        }
        else {
            columns[i].srcindex = -1;
            columns[i].data = _extract_column(self, j, rowmapping);
            if (columns[i].data == NULL) goto fail;
        }
    }

    DataTable *res = malloc(sizeof(DataTable));
    if (res == NULL) return NULL;

    res->nrows = nrows;
    res->ncols = ncols;
    res->source = self->source != NULL? self->source : self;
    res->rowmapping = merged_rowindex != NULL? merged_rowindex : rowmapping;
    res->columns = columns;
    return res;

  fail:
    free(columns);
    RowMapping_dealloc(merged_rowindex);
    return NULL;
}


/**
 * Copy data from i-th column in the datatable `dt` into a newly allocated
 * array. The caller is given ownership of this new array. The data is
 * extracted according to the provided `rowmapping`.
 */
static void* _extract_column(DataTable *dt, int64_t i, RowMapping *rowmapping)
{
    uint64_t n = (uint64_t) rowmapping->length;
    DataSType stype = dt->columns[i].stype;
    void *coldata = dt->columns[i].data;
    assert(coldata != NULL);

    size_t elemsize = stype_info[stype].elemsize;
    if (stype == ST_STRING_FCHAR) {
        elemsize = ((FixcharMeta*)dt->columns[i].meta)->n;
    }
    void *newdata = malloc((size_t)n * elemsize);
    if (newdata == NULL) return NULL;
    if (rowmapping->type == RI_SLICE) {
        uint64_t start = (uint64_t) rowmapping->slice.start;
        int64_t step = rowmapping->slice.step;
        if (step == 1) {
            memcpy(newdata, coldata + start * elemsize, n * elemsize);
        } else {
            void *newdataptr = newdata;
            void *sourceptr = coldata + start * elemsize;
            int64_t stepsize = step * (int64_t)elemsize;
            for (uint64_t j = 0; j < n; j++) {
                memcpy(newdataptr, sourceptr, elemsize);
                newdataptr += elemsize;
                sourceptr += stepsize;
            }
        }
    }
    else if (rowmapping->type == RI_ARRAY) {
        void *newdataptr = newdata;
        int64_t *rowindices = rowmapping->indices;
        for (uint64_t j = 0; j < n; j++) {
            memcpy(newdataptr, coldata + (uint64_t)rowindices[j] * elemsize, elemsize);
            newdataptr += elemsize;
        }
    }
    else assert(0);

    return newdata;
}


/**
 * Free memory occupied by this DataTable object. This function should be
 * called from `DataTable_PyObject`s deallocator only.
 *
 * :param dealloc_col
 *     Pointer to a function that will be used to clean up data within columns
 *     of type LT_OBJECT (these objects are Python-native). The function will
 *     be passed two parameters: a `void*` pointer to the underlying data buffer
 *     and the number of rows in the column.
 */
void dt_DataTable_dealloc(DataTable *self, objcol_deallocator *dealloc_col)
{
    if (self == NULL) return;

    self->source = NULL;  // .source reference is not owned by this object
    RowMapping_dealloc(self->rowmapping);
    self->rowmapping = NULL;
    int64_t i = self->ncols;
    while (--i >= 0) {
        Column column = self->columns[i];
        if (column.data != NULL) {
            // if (column.stype == LT_OBJECT) {
            //     (*dealloc_col)(column.data, self->nrows);
            // }
            if (column.mmapped) {
                size_t elemsize = stype_info[column.stype].elemsize;
                munmap(column.data, elemsize * (size_t)self->nrows);
            } else {
                free(column.data);
            }
        }
        // free(column.meta);
    }
    free(self->columns);
    free(self);
}
