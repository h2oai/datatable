#ifndef dt_DATATABLE_H
#define dt_DATATABLE_H
#include <inttypes.h>
#include "coltype.h"

// break circular dependency between .h files
typedef struct RowMapping RowMapping;
typedef struct Column Column;


/*----------------------------------------------------------------------------*/
/**
 * The DataTable
 *
 * :param nrows, ncols:
 *     Data dimensions: number of rows and number of columns. We do not support
 *     more than 2 dimensions as Numpy or TensorFlow do.
 *
 * :param source:
 *     If this field is not NULL, then the current datatable is a view on the
 *     referenced datatable. The referenced datatable cannot be also a view,
 *     that is the following invariant holds:
 *         self->source == NULL || self->source->source == NULL
 *     This reference is *not* owned by the current datatable, however it is
 *     mirrored in the controller DataTable_PyObject.
 *
 * :param rowmapping:
 *     This field is present if and only if the datatable is a view (i.e.
 *     `source` != NULL). In this case `rowmapping` describes which rows from the
 *     source datatable are "selected" into the current datatable.
 *     This reference is owned by the current datatable (in particular you
 *     should not construct a RowMapping_PyObject from it).
 *
 * :param columns:
 *     The array of columns within the datatable. This array contains `ncols`
 *     elements, and each column has the same number of rows: `nrows`.
 */
typedef struct DataTable {
    int64_t nrows;
    int64_t ncols;
    struct DataTable *source;
    struct RowMapping *rowmapping;
    struct Column *columns;

} DataTable;



/*----------------------------------------------------------------------------*/
/**
 * Single column within a datatable.
 *
 * A column can be of two kinds: a "data" column, and a "view" column. A "data"
 * column will have non-NULL `data` array storing the values of this column.
 * Such columns may exist both in regular datatables and in view datatables.
 * The value in `j`-th row of this column is always `((dtype*)data)[j]`, where
 * `dtype` is the storage type for this column. This expression is valid even
 * in view datatables.
 * A "view" column may only exist in a view datatable, and it is characterized
 * by NULL `data` field. Then the `srcindex` attribute gives the index of the
 * column in the `source` datatable to which the current column defers. The
 * values from the "source" column must be extracted according to the current
 * datatable's `rowmapping`.
 *
 * :param data:
 *     Raw data storage. This is a plain array with `nrows` elements, where the
 *     type of each element depends on the column's `type`. This can also be
 *     NULL (when the datatable is a view), indicating that this columns data
 *     is in the `source` datatable.
 *
 * :param type:
 *     Type of data within the column (the enum is defined in coltype.h).
 *     Here's the correspondence between the values of the `type` field and
 *     the type of the `data` storage:
 *         DT_AUTO:    --
 *         DT_DOUBLE:  double
 *         DT_LONG:    int64_t
 *         DT_STRING:  ?
 *         DT_BOOL:    char
 *         DT_OBJECT:  PyObject*
 *
 * :param srcindex:
 *     This parameter is meaningful only if `data` is NULL. In that case it
 *     gives the index of the column in the `source` datatable that the current
 *     column references. The type of the referenced column should coincide
 *     with the current column's `type`.
 */
typedef struct Column {
    void* data;
    enum ColType type;
    int64_t srcindex;
    // RollupStats* stats;
} Column;



/*---- Methods ---------------------------------------------------------------*/
typedef void objcol_deallocator(void*, int64_t);

DataTable* dt_DataTable_call(DataTable *self, RowMapping *rowmapping);
void dt_DataTable_dealloc(DataTable *self, objcol_deallocator *dealloc_col);


#endif
