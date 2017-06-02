#include <stdlib.h>
#include "datatable.h"
#include "py_utils.h"
#include "rowmapping.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);


/**
 * Create new DataTable given its number of rows, and the array of `Column`
 * objects.
 */
DataTable* datatable_assemble(RowMapping *rowmapping, Column **cols)
{
    if (cols == NULL || rowmapping == NULL) return NULL;
    int64_t ncols = 0;
    while(cols[ncols] != NULL) ncols++;

    DataTable *res = NULL;
    dtmalloc(res, DataTable, 1);
    res->nrows = rowmapping->length;
    res->ncols = ncols;
    res->rowmapping = rowmapping;
    res->columns = cols;
    return res;
}



/**
 *
 */
DataTable* dt_delete_columns(DataTable *dt, int *cols_to_remove, int n)
{
    if (n == 0) return dt;
    qsort(cols_to_remove, (size_t)n, sizeof(int), _compare_ints);
    Column **columns = dt->columns;
    int j = 0;
    int next_col_to_remove = cols_to_remove[0];
    int k = 0;
    for (int i = 0; i <= dt->ncols; i++) {
        if (i == next_col_to_remove) {
            column_decref(columns[i]);
            columns[i] = NULL;
            do {
                k++;
                next_col_to_remove = k < n? cols_to_remove[k] : -1;
            } while (next_col_to_remove == i);
        } else {
            columns[j++] = columns[i];
        }
    }
    // This may not be the same as `j` if there were repeating columns
    dt->ncols = j - 1;
    dtrealloc(dt->columns, Column*, j);

    return dt;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `DataTable_PyObject`s deallocator only.
 */
void datatable_dealloc(DataTable *self)
{
    if (self == NULL) return;

    rowmapping_dealloc(self->rowmapping);
    for (int64_t i = 0; i < self->ncols; i++) {
        column_decref(self->columns[i]);
    }
    free(self->columns);
    free(self);
}


// Comparator function to sort integers using `qsort`
static int _compare_ints(const void *a, const void *b) {
    const int x = *(const int*)a;
    const int y = *(const int*)b;
    return (x > y) - (x < y);
}
