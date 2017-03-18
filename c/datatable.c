#include "Python.h"
#include "structmember.h"
#include "limits.h"
#include "datatable.h"
#include "dtutils.h"
#include "rows.h"
#include "py_datawindow.h"
#include "py_rowindex.h"


/**
 * Main "driver" function for the DataTable. This function corresponds to
 * DataTable.__call__ in Python.
 */
DataTable* dt_DataTable_call(DataTable *self, RowIndex *rowindex)
{
    int64_t ncols = self->ncols;
    int64_t nrows = rowindex->length;

    Column *columns = malloc(sizeof(Column) * ncols);
    if (columns == NULL) return NULL;
    for (int i = 0; i < ncols; ++i) {
        columns[i].data = NULL;
        columns[i].srcindex = i;
        columns[i].type = self->columns[i].type;
    }

    DataTable *res = malloc(sizeof(DataTable));
    if (res == NULL) return NULL;

    res->nrows = nrows;
    res->ncols = ncols;
    res->source = self->source == NULL? self : self->source;
    res->rowindex = rowindex;
    res->columns = columns;
    return res;
}


/**
 * Free memory occupied by this DataTable object. This function should be
 * called from `DataTable_PyObject`s deallocator only.
 *
 * :param dealloc_col
 *     Pointer to a function that will be used to clean up data within columns
 *     of type DT_OBJECT (these objects are Python-native). The function will
 *     be passed two parameters: a `void*` pointer to the underlying data buffer
 *     and the number of rows in the column.
 */
void dt_DataTable_dealloc(DataTable *self, objcol_deallocator *dealloc_col)
{
    if (self == NULL) return;

    self->source = NULL;  // .source reference is not owned by this object
    dt_RowIndex_dealloc(self->rowindex);
    self->rowindex = NULL;
    int64_t i = self->ncols;
    while (--i >= 0) {
        Column column = self->columns[i];
        if (column.data != NULL) {
            if (column.type == DT_OBJECT) {
                (*dealloc_col)(column.data, self->nrows);
            }
            free(column.data);
        }
    }
    free(self->columns);
    free(self);
}
