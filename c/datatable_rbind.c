#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "column.h"
#include "datatable.h"
#include "rowmapping.h"
#include "utils.h"


// Forward declarations
static Column* rbind_string_column(Column *col, DataTable *dt, DataTable **dts,
                                   int *cols, int ndts);
static Column* rbind_fixedwidth_column(
    Column *col, DataTable *dt, DataTable **dts, int *cols, int ndts,
    int64_t nrows
);
static void set_value(void * restrict ptr, const void * restrict value,
                      size_t sz, size_t count);



/**
 * Append to datatable `dt` a list of datatables `dts`. There are `ndts` items
 * in this list. The `cols` array of dimensions `ncols x ndts` specifies how
 * the columns should be matched.
 * In particular, the datatable `dt` will be expanded to have `ncols` columns,
 * and `dt->nrows + sum(dti->nrows for dti in dts)` rows. The `i`th column
 * in the expanded datatable will have the following structure: first comes the
 * data from `i`th column of `dt` (if `i < dt->ncols`, otherwise NAs); after
 * that come `ndts` blocks of rows, each `j`th block having data from column
 * number `cols[i][j]` in datatable `dts[j]` (if `cols[i][j] >= 0`, otherwise
 * NAs).
 */
DataTable*
dt_rbind(DataTable *dt, DataTable **dts, int **cols, int ndts, int ncols)
{
    assert(ncols >= dt->ncols);
    dtrealloc(dt->columns, Column*, ncols + 1);
    dt->columns[ncols] = NULL;

    int64_t nrows = dt->nrows;
    for (int i = 0; i < ndts; i++) {
        nrows += dts[i]->nrows;
    }

    // TODO: instantiate all view datatables (unless they are slice views with
    // step 1). If `dt` is a view, it must also be instantiated.

    for (int i = 0; i < ncols; i++) {
        Column *col = NULL;
        if (i >= dt->ncols) {
            dtmalloc(col, Column, 1);
            col->data = NULL;
            col->meta = NULL;
            col->alloc_size = 0;
            col->stype = 0;
            col->mtype = MT_DATA;
            for (int j = 0; j < ndts; j++) {
                if (cols[i][j] >= 0) {
                    col->stype = dts[j]->columns[cols[i][j]]->stype;
                    break;
                }
            }
            dt->columns[i] = col;
        } else {
            col = dt->columns[i];
        }

        // TODO: determine the column's final stype, taking into account stypes
        // of all columns being appended. Convert all datatables into the common
        // stype.

        Column *ret = NULL;
        if (!stype_info[col->stype].varwidth) {
            ret = rbind_fixedwidth_column(col, dt, dts, cols[i], ndts, nrows);
        } else if (col->stype == ST_STRING_I4_VCHAR) {
            ret = rbind_string_column(col, dt, dts, cols[i], ndts);
        } else {
            // Cannot handle this column type
            return NULL;
        }
        if (!ret) return NULL;
    }
    dt->ncols = ncols;
    dt->nrows = nrows;

    return dt;
}



static Column* rbind_fixedwidth_column(
    Column *col, DataTable *dt, DataTable **dts, int *cols, int ndts,
    int64_t nrows
) {
    int nocol = col->alloc_size == 0;

    // Determine the NA value for this column, if applicable
    const void *na = stype_info[col->stype].na;
    unsigned char *tmp_na_val = NULL;
    size_t elemsize = stype_info[col->stype].elemsize;
    if (col->stype == ST_STRING_FCHAR) {
        elemsize = (size_t) ((FixcharMeta*)(col->meta))->n;
        dtmalloc(tmp_na_val, unsigned char, 1);
        memset(tmp_na_val, 0xFF, elemsize);
        na = (const void*) tmp_na_val;
    }

    // Determine the new allocation size
    size_t old_alloc_size = col->alloc_size;
    size_t new_alloc_size = elemsize * (size_t) nrows;
    dtrealloc_v(col->data, new_alloc_size);
    col->alloc_size = new_alloc_size;

    // Copy the data
    void *colptr = nocol? col->data : col->data + old_alloc_size;
    size_t rows_to_fill = nocol? (size_t) dt->nrows : 0;
    for (int i = 0; i < ndts; i++) {
        if (cols[i] < 0) {
            rows_to_fill += (size_t) dts[i]->nrows;
        } else {
            if (rows_to_fill) {
                set_value(colptr, na, elemsize, rows_to_fill);
                colptr += rows_to_fill * elemsize;
                rows_to_fill = 0;
            }
            // TODO: take into account views
            Column *coli = dts[i]->columns[cols[i]];
            memcpy(colptr, coli->data, coli->alloc_size);
            colptr += coli->alloc_size;
        }
    }
    if (rows_to_fill) {
        set_value(colptr, na, elemsize, rows_to_fill);
    }

    // Done.
    dtfree(tmp_na_val);
    return col;
}



/**
 * Helper method to append a single column of ST_STRING_I4_VCHAR type. See the
 * description of this type in `types.h`, but in a nutshell such column consists
 * of 2 regions: first comes the string data, then the offsets (there is also a
 * small gap between these two regions, to ensure that the offsets start at a
 * memory address that is aligned to 8-byte width.
 * The meta information structure for this column contains the offset of the
 * "offsets" region.
 */
static Column* rbind_string_column(
    Column *col, DataTable *dt, DataTable **dts, int *cols, int ndts
) {
    size_t elemsize = stype_info[col->stype].elemsize;
    assert(col->stype == ST_STRING_I4_VCHAR);
    assert(elemsize == 4);

    // Create the `col->meta` structure if necessary (which happens when the
    // column did not exist in the original datatable).
    int nocol = (col->meta == NULL);
    if (nocol) {
        dtmalloc(col->meta, VarcharMeta, 1);
        ((VarcharMeta*) col->meta)->offoff = 0;
    }
    VarcharMeta *meta = (VarcharMeta*) col->meta;

    // Determine the "map" of which chunks of data have to be copied
    typedef struct { int32_t off0, off1, row0, row1; } tmap;
    tmap *map = NULL;
    dtmalloc(map, tmap, ndts + 1);
    {
        map[0].row0 = 0;
        map[0].row1 = (int32_t) dt->nrows;
        if (nocol) {
            map[0].off0 = -1;
            map[0].off1 = -1;
        } else {
            int32_t *offsets = (int32_t*) add_ptr(col->data, meta->offoff);
            map[0].off0 = map[0].row0? abs(offsets[map[0].row0 - 1]) - 1 : 0;
            map[0].off1 = abs(offsets[map[0].row1 - 1]) - 1;
        }
    }
    for (int i = 1; i <= ndts; i++) {
        DataTable *dti = dts[i - 1];
        map[i].row0 = 0;
        map[i].row1 = (int32_t) dti->nrows;
        if (cols[i - 1] < 0) {
            map[i].off0 = -1;
            map[i].off1 = -1;
        } else {
            if (dti->rowmapping) {
                assert(dti->rowmapping->type == RM_SLICE);
                assert(dti->rowmapping->slice.step == 1);
                map[i].row0 = (int32_t) dti->rowmapping->slice.start;
                map[i].row1 = (int32_t) dti->rowmapping->length;
            }
            Column *coli = dti->columns[cols[i - 1]];
            VarcharMeta *metai = (VarcharMeta*) coli->meta;
            int32_t *offsets = (int32_t*) add_ptr(coli->data, metai->offoff);
            map[i].off0 = map[i].row0? abs(offsets[map[i].row0 - 1]) - 1 : 0;
            map[i].off1 = abs(offsets[map[i].row1 - 1]) - 1;
        }
    }

    // Determine the size of the memory to allocate
    size_t new_data_size = 0;     // size of the string data region
    size_t new_offsets_size = 0;  // size of the offsets region
    for (int i = 0; i <= ndts; i++) {
        new_data_size += (size_t) (map[i].off1 - map[i].off0);
        new_offsets_size += (size_t) (map[i].row1 - map[i].row0) * elemsize;
    }
    size_t padding_size = (8 - (new_data_size & 7)) & 7;
    size_t new_offoff = new_data_size + padding_size;
    size_t new_alloc_size = new_offoff + new_offsets_size;

    // Reallocate the column
    dtrealloc(col->data, char, new_alloc_size);
    col->alloc_size = new_alloc_size;
    if (!nocol) {
        memmove(add_ptr(col->data, new_offoff),
                add_ptr(col->data, meta->offoff),
                (size_t) dt->nrows * elemsize);
    }
    meta->offoff = (int64_t) new_offoff;

    // Copy the data and remap the offsets
    int32_t *offsets = (int32_t*) add_ptr(col->data, new_offoff);
    int32_t rows_to_fill = 0;  // how many rows need to be filled with NAs
    int32_t curr_offset = 0;
    if (map[0].off0 < 0) {
        rows_to_fill += map[0].row1 - map[0].row0;
    } else {
        curr_offset += map[0].off1 - map[0].off0;
        offsets += map[0].row1 - map[0].row0;
    }
    for (int i = 1; i <= ndts; i++) {
        if (map[i].off0 < 0) {
            rows_to_fill += map[i].row1 - map[i].row0;
        } else {
            if (rows_to_fill) {
                const int32_t na = -curr_offset;
                set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
                offsets += rows_to_fill;
                rows_to_fill = 0;
            }
            Column *coli = dts[i-1]->columns[cols[i-1]];
            memcpy(add_ptr(col->data, curr_offset),
                   add_ptr(coli->data, map[i].off0),
                   (size_t)(map[i].off1 - map[i].off0));
            int64_t offoffi = ((VarcharMeta*) coli->meta)->offoff;
            int32_t *dti_offsets = (int32_t*) add_ptr(coli->data, offoffi);
            int32_t row0 = map[i].row0;
            int32_t row1 = map[i].row1;
            int32_t delta = curr_offset + 1 - abs(dti_offsets[row0]);
            for (int32_t j = row0; j < row1; j++) {
                int32_t off = dti_offsets[j];
                *offsets++ = off > 0? off + delta : off - delta;
            }
            curr_offset += map[i].off1 - map[i].off0;
        }
    }
    if (rows_to_fill) {
        const int32_t na = -curr_offset;
        set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
    }
    if (padding_size) {
        memset(add_ptr(col->data, new_offoff - padding_size), 0xFF,
               padding_size);
    }

    free(map);
    return col;
}



static inline size_t min1(size_t x, size_t y) {
    return x < y? x : y;
}

/**
 * Fill the array `ptr` with `count` values `value` (the value is given as a
 * pointer of size `sz`).
 * This is used for filling the columns with NAs.
 */
static void set_value(void * restrict ptr, const void * restrict value,
                      size_t sz, size_t count)
{
    memcpy(ptr, value, sz);
    size_t final_sz = sz * count;
    for (size_t i = sz; i < final_sz; i <<= 1) {
        size_t writesz = min1(i, final_sz - i);
        memcpy(ptr + i, ptr, writesz);
    }
}
