#include <stdlib.h>
#include "myomp.h"
#include "datatable.h"
#include "py_utils.h"
#include "rowindex.h"

// Forward declarations
static int _compare_ints(const void *a, const void *b);


/**
 * Create new DataTable given the set of columns, and a rowindex. The `rowindex`
 * object may also be NULL, in which case a DataTable without a rowindex will
 * be constructed.
 */
DataTable* make_datatable(Column **cols, RowIndex *rowindex)
{
    if (cols == NULL) return NULL;
    int64_t ncols = 0;
    while(cols[ncols] != NULL) ncols++;
    DataTable *res = NULL;
    dtmalloc(res, DataTable, 1);
    res->nrows = 0;
    res->ncols = ncols;
    res->rowindex = NULL;
    res->columns = cols;
    res->stats = NULL;
    if (rowindex) {
        res->rowindex = rowindex;
        res->nrows = rowindex->length;
        dtcalloc(res->stats, Stats*, res->ncols);
        for (int64_t i = 0; i < res->ncols; ++i) {
            res->stats[i] = Stats::void_ptr();
        }
    } else if (ncols) {
        res->nrows = cols[0]->nrows;
    }
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
    Stats **stats = dt->stats;
    int j = 0;
    int next_col_to_remove = cols_to_remove[0];
    int k = 0;
    for (int i = 0; i < dt->ncols; i++) {
        if (i == next_col_to_remove) {
            column_decref(columns[i]);
            columns[i] = NULL;
            if (stats) Stats::destruct(stats[i]);
            do {
                k++;
                next_col_to_remove = k < n? cols_to_remove[k] : -1;
            } while (next_col_to_remove == i);
        } else {
            if (stats) stats[j] = stats[i];
            columns[j] = columns[i];
            j++;
        }
    }
    columns[j] = NULL;
    // This may not be the same as `j` if there were repeating columns
    dt->ncols = j;
    dtrealloc(dt->columns, Column*, j + 1);
    if (stats) dtrealloc(dt->stats, Stats*, j);
    return dt;
}



/**
 * Free memory occupied by the :class:`DataTable` object. This function should
 * be called from `DataTable_PyObject`s deallocator only.
 */
void datatable_dealloc(DataTable *self)
{
    if (self == NULL) return;

    rowindex_decref(self->rowindex);
    for (int64_t i = 0; i < self->ncols; ++i) {
        column_decref(self->columns[i]);
    }
    dtfree(self->columns);
    if (self->stats) {
        for (int64_t i = 0; i < self->ncols; ++i) {
            Stats::destruct(self->stats[i]);
        }
        dtfree(self->stats);
    }
    dtfree(self);
}


// Comparator function to sort integers using `qsort`
static int _compare_ints(const void *a, const void *b) {
    const int x = *(const int*)a;
    const int y = *(const int*)b;
    return (x > y) - (x < y);
}


/**
 * Modify datatable replacing values that are given by the mask with NAs.
 * The target datatable must have the same shape as the mask, and neither can
 * be a view.
 * Returns NULL in case of an error, or a pointer to `dt` otherwise.
 */
DataTable* datatable_apply_na_mask(DataTable *dt, DataTable *mask)
{
    if (dt == NULL || mask == NULL) return NULL;
    if (dt->ncols != mask->ncols || dt->nrows != mask->nrows) {
        dterrr0("Target datatable and mask have different shapes");
    }
    if (dt->rowindex || mask->rowindex) {
        dterrr0("Neither target datatable nor a mask can be views");
    }
    int ncols = (int) dt->ncols;
    for (int i = 0; i < ncols; i++) {
        if (mask->columns[i]->stype != ST_BOOLEAN_I1)
            dterrv("Column %d in mask is not of a boolean type", i);
    }

    int64_t nrows = dt->nrows;
    for (int i = 0; i < ncols; i++) {
        // TODO: Move this part into columns.c?
        Column *col = dt->columns[i];
        Stats::destruct(col->stats);
        col->stats = Stats::void_ptr();
        uint8_t *mdata = (uint8_t*) mask->columns[i]->data;
        switch (col->stype) {
            case ST_BOOLEAN_I1:
            case ST_INTEGER_I1: {
                uint8_t *cdata = (uint8_t*) col->data;
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; j++) {
                    if (mdata[j]) cdata[j] = (uint8_t)NA_I1;
                }
                break;
            }
            case ST_INTEGER_I2: {
                uint16_t *cdata = (uint16_t*) col->data;
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; j++) {
                    if (mdata[j]) cdata[j] = (uint16_t)NA_I2;
                }
                break;
            }
            case ST_REAL_F4:
            case ST_INTEGER_I4: {
                uint32_t *cdata = (uint32_t*) col->data;
                uint32_t na = col->stype == ST_REAL_F4 ? NA_F4_BITS
                                                       : (uint32_t)NA_I4;
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; j++) {
                    if (mdata[j]) cdata[j] = na;
                }
                break;
            }
            case ST_REAL_F8:
            case ST_INTEGER_I8: {
                uint64_t *cdata = (uint64_t*) col->data;
                uint64_t na = col->stype == ST_REAL_F8 ? NA_F8_BITS
                                                       : (uint64_t)NA_I8;
                #pragma omp parallel for schedule(dynamic,1024)
                for (int64_t j = 0; j < nrows; j++) {
                    if (mdata[j]) cdata[j] = na;
                }
                break;
            }
            case ST_STRING_I4_VCHAR: {
                int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
                char *strdata = (char*)(col->data) - 1;
                int32_t *offdata = (int32_t*) add_ptr(col->data, offoff);
                // How much to reduce the offsets by due to some strings being
                // converted into NAs
                int32_t doffset = 0;
                for (int64_t j = 0; j < nrows; j++) {
                    int32_t offi = offdata[j];
                    int32_t offp = abs(offdata[j - 1]);
                    if (mdata[j]) {
                        doffset += abs(offi) - offp;
                        offdata[j] = -offp;
                    } else if (doffset) {
                        if (offi > 0) {
                            offdata[j] = offi - doffset;
                            memmove(strdata + offp, strdata + offp + doffset,
                                    (size_t)(offi - offp - doffset));
                        } else {
                            offdata[j] = -offp;
                        }
                    }
                }
                break;
            }
            default:
                dterrr("Column type %d not supported in apply_mask", col->stype);
        }

    }

    return dt;
}

/**
 * Convert a DataTable view into an actual DataTable. This is done in-place.
 * The resulting DataTable should have a NULL RowIndex and Stats array.
 * Do nothing if the DataTable is not a view.
 */
void datatable_reify(DataTable *self) {
    if (self->rowindex == NULL) return;
    for (int64_t i = 0; i < self->ncols; ++i) {
        Column *newcol = column_extract(self->columns[i], self->rowindex);
        if (!self->stats[i]->is_void()) {
            newcol->stats = self->stats[i];
            newcol->stats->_ref_col = newcol;
            newcol->stats->_ref_ri = NULL;
        }
        column_decref(self->columns[i]);
        self->columns[i] = newcol;
    }
    rowindex_decref(self->rowindex);
    dtfree(self->stats);
    self->rowindex = NULL;
    self->stats = NULL;
}



size_t datatable_get_allocsize(DataTable *self)
{
    size_t sz = 0;
    sz += sizeof(DataTable);
    sz += (size_t)(self->ncols + 1) * sizeof(Column*);
    if (self->rowindex) {
        // If table is a view, then ignore sizes of each individual column.
        sz += rowindex_get_allocsize(self->rowindex);
    } else {
        for (int i = 0; i < self->ncols; i++) {
            sz += column_get_allocsize(self->columns[i]);
        }
    }
    if (self->stats != NULL) {
        sz += (size_t)(self->ncols) * sizeof(Stats*);
        for (int64_t i = 0; i < self->ncols; ++i) {
            if (self->stats[i])
            sz += self->stats[i]->alloc_size();
        }
    }
    return sz;
}
