#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include "types.h"
#include "column.h"

// avoid circular dependency between .h files
typedef struct RowIndex RowIndex;
typedef struct ColMapping ColMapping;
typedef struct Column Column;
typedef struct DataTable DataTable;



//==============================================================================

/**
 * The DataTable
 *
 * Parameters
 * ----------
 * nrows
 * ncols
 *     Data dimensions: number of rows and columns in the datatable. We do not
 *     support more than 2 dimensions (as Numpy or TensorFlow do).
 *     The maximum number of rows is 2**63 - 1. The maximum number of columns
 *     is 2**31 - 1 (even though `ncols` is declared as `int64_t`).
 *
 * rowindex
 *     If this field is not NULL, then the current datatable is a "view", that
 *     is, all columns should be accessed not directly but via this rowindex.
 *     When this field is set, it must be that `nrows == rowindex->length`.
 *
 * columns
 *     The array of columns within the datatable. This array contains `ncols+1`
 *     elements, and each column has the same number of rows: `nrows`. The
 *     "extra" column is always NULL: a sentinel.
 *     When `rowindex` is specified, then each column is a copy-by-reference
 *     of a column in some other datatable, and only indices given in the
 *     `rowindex` should be used to access values in each column.
 */
typedef struct DataTable {
    int64_t     nrows;
    int64_t     ncols;
    RowIndex *rowindex;
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
DataTable* datatable_assemble(RowIndex *rowindex, Column **cols);
DataTable* datatable_load(DataTable *colspec, int64_t nrows);
DataTable* dt_delete_columns(DataTable *dt, int *cols_to_remove, int n);
DataTable* dt_rbind(DataTable *dt, DataTable **dts, int **cols, int ndts, int ncols);

void datatable_dealloc(DataTable *self);


#endif
