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
 * rowmapping
 *     If this field is not NULL, then the current datatable is a "view", that
 *     is all columns should be accessed not directly but via this rowmapping.
 *     This pointer is owned by the current datatable (in particular you should
 *     not construct a RowMapping_PyObject from it).
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
    RowMapping *rowmapping;
    Column    **columns;

} DataTable;



//==============================================================================
// Auxiliary definitions

// Status codes for the function verify_integrity()
#define DTCK_NOERRORS       0
#define DTCK_ERRORS_FOUND   1
#define DTCK_ERRORS_FIXED   2
#define DTCK_RUNTIME_ERROR  4
#define DTCK_CANCELLED      8


//==============================================================================

int dt_verify_integrity(DataTable *dt, char **errors, _Bool fix);
DataTable* make_datatable(int64_t nrows, Column **cols);
DataTable* datatable_assemble(RowMapping *rowmapping, Column **cols);
DataTable* dt_delete_columns(DataTable *dt, int *cols_to_remove, int n);
DataTable* dt_rbind(DataTable *dt, DataTable **dts, int **cols, int ndts, int ncols);

void datatable_dealloc(DataTable *self);


#endif
