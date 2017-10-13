#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include "stats.h"
#include "types.h"
#include "column.h"

// avoid circular dependency between .h files
class RowIndex;
typedef struct ColMapping ColMapping;
class Column;;
class Stats;
class DataTable;



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
class DataTable {
public:
    int64_t     nrows;
    int64_t     ncols;
    RowIndex   *rowindex;
    Column    **columns;

    DataTable(Column**);
    ~DataTable();
    DataTable* delete_columns(int*, int);
    void apply_na_mask(DataTable* mask);
    void reify();
    DataTable* rbind(DataTable**, int**, int, int64_t);
    DataTable* cbind(DataTable**, int);
    size_t memory_footprint();
    int verify_integrity(char**);

    static DataTable* load(DataTable*, int64_t);
};



//==============================================================================

#endif
