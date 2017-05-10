#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include "types.h"
#include "column.h"

// avoid circular dependency between .h files
typedef struct RowMapping RowMapping;
typedef struct ColMapping ColMapping;
typedef struct Column Column;
typedef struct DataTable DataTable;



//==============================================================================

/**
 * The DataTable
 *
 * nrows
 * ncols
 *     Data dimensions: number of rows and columns in the datatable. We do not
 *     support more than 2 dimensions as Numpy or TensorFlow.
 *     On 32-bit platforms maximum number of rows/columns is 2**31-1. On 64-bit
 *     platforms the maximum is 2**63-1.
 *
 * source
 *     If this field is not NULL, then the current datatable is a view on the
 *     ``source`` datatable. The referenced datatable cannot be also a view,
 *     that is the following invariant holds:
 *
 *         (self->source == NULL) || (self->source->source == NULL)
 *
 *     This reference is *not* owned by the current datatable, however it is
 *     mirrored in the controller DataTable_PyObject.
 *
 * rowmapping
 *     This field is present if and only if the datatable is a view, i.e. the
 *     following invariant must hold:
 *
 *         (self->source == NULL) == (self->rowmapping == NULL)
 *
 *     This field describes which rows from the source datatable are "selected"
 *     into the current datatable. This reference is owned by the current
 *     datatable (in particular you should not construct a RowMapping_PyObject
 *     from it).
 *
 * columns
 *     The array of columns within the datatable. This array contains `ncols`
 *     elements, and each column has the same number of rows: `nrows`. Even
 *     though each column is declared to have type `Column*`, in reality it
 *     may also be of type `ViewColumn*`. Both structures have field `mtype`
 *     which determines the actual struct type.
 */
typedef struct DataTable {
    int64_t     nrows;
    int64_t     ncols;
    DataTable  *source;
    RowMapping *rowmapping;
    Column    **columns;

} DataTable;



//==============================================================================

DataTable* dt_DataTable_call(
    DataTable *self, RowMapping *rowmapping, ColMapping *colmapping);

void datatable_dealloc(DataTable *self);


#endif
