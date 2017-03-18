#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include "coltype.h"

typedef struct RowIndex RowIndex; // break circular dependency between .h files
typedef struct Column Column;


/*----------------------------------------------------------------------------*/
/**
 * The DataTable
 *
 * `nrows`, `ncols`
 *     Data dimensions: number of rows and number of columns. We do not support
 *     more than 2 dimensions as Numpy or TensorFlow do.
 *
 * `source`
 *     If this field is not NULL, then the current datatable is a view on the
 *     referenced datatable. The referenced datatable cannot be also a view,
 *     that is the following invariant holds:
 *         self->source == NULL || self->source->source == NULL
 *     This reference is *not* owned by the current datatable, however it is
 *     mirrored in the controller DataTable_PyObject.
 *
 * `rowindex`
 *     This field is present if and only if the datatable is a view (i.e.
 *     `source` != NULL). In this case `rowindex` describes which rows from the
 *     source datatable are "selected" into the current datatable.
 *     This reference is owned by the current datatable (in particular you
 *     should not construct a RowIndex_PyObject from it).
 *
 * `columns`
 *     The array of columns within the datatable. This array contains `ncols`
 *     elements, and each column has the same number of rows: `nrows`.
 */
typedef struct DataTable {
    int64_t nrows;
    int64_t ncols;
    struct DataTable *source;
    struct RowIndex *rowindex;
    struct Column *columns;

} DataTable;



/*----------------------------------------------------------------------------*/
/**
 * Single column within a datatable.
 *
 */
typedef struct Column {
    void* data;
    enum ColType type;
    int64_t srcindex;
    // RollupStats* stats;
} Column;



/*---- Methods ---------------------------------------------------------------*/
typedef void objcol_deallocator(void*, int64_t);

DataTable* dt_DataTable_call(DataTable *self, RowIndex *rowindex);
void dt_DataTable_dealloc(DataTable *self, objcol_deallocator *dealloc_col);


#endif
