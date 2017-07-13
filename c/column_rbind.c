#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include <stdlib.h>  // abs
#include "column.h"
#include "myassert.h"
#include "types.h"
#include "utils.h"

// Forward declarations
static void set_value(void * restrict ptr, const void * restrict value, size_t sz, size_t count);
static Column* column_rbind_fw(Column *self, Column **cols, int64_t nrows);
static Column* column_rbind_str32(Column *col, Column **cols, int64_t nrows);



/**
 * Append columns `cols` to the bottom of the current column -- equivalent of
 * Python's `list.append()` or R's `rbind()`.
 *
 * If possible, the current column will be modified in-place (which is a common
 * use-case). Otherwise, a new Column object will be created and returned,
 * while `self` will be dec-refed.
 *
 * This function also takes ownership of the `cols` array, deallocating it in
 * the end.
 *
 * Any of the columns passed to this function may have stype = 0, which means
 * that it is a column containing only NAs. Also, the array `cols` should be
 * NULL-terminating.
 */
Column* column_rbind(Column *self, Column **cols)
{
    // Compute the final number of rows and stype
    int64_t nrows = self->nrows;
    int64_t nrows0 = nrows;
    SType stype = self->stype;
    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        nrows += col->nrows;
        if (stype < col->stype) stype = col->stype;
    }
    if (stype == 0) stype = ST_BOOLEAN_I1;

    // Create the resulting Column object. It can be either: an empty column
    // filled with NAs; the current column (`self`); a clone of the current
    // column (if it has refcount > 1); or a type-cast of the current column.
    Column *res = NULL;
    if (self->stype == 0) {
        res = make_data_column(stype, (size_t) self->nrows);
        res->refcount = 0;
    } else if (self->refcount == 1 && self->mtype == MT_DATA &&
               self->stype == stype) {
        // Happy place: current column can be modified in-place.
        res = column_incref(self);
    } else {
        res = (self->stype == stype) ? column_copy(self)
                                     : column_cast(self, stype);
    }
    if (res == NULL) return NULL;
    column_decref(self);
    assert(res->stype == stype && res->refcount <= 1 &&
           res->mtype == MT_DATA && res->nrows == nrows0);

    // Use the appropriate strategy to continue appending the columns.
    if (!stype_info[stype].varwidth) {
        return column_rbind_fw(res, cols, nrows);
    } else if (stype == ST_STRING_I4_VCHAR) {
        return column_rbind_str32(res, cols, nrows);
    }
    return NULL;
}



/**
 * Helper method designed specifically to append columns of fixed-width stypes.
 * The current column (`self`) will be modified in-place, and must already be
 * "clean", i.e. have refcount 1 and memory type MT_DATA. As a special case, if
 * the current column is empty and haven't been filled with NAs yet, then its
 * refcount will be set to 0.
 *
 * The `cols` array has the same meaning as in `column_rbind()`. Lastly, `nrows`
 * is the final row count for the column being constructed.
 */
static Column*
column_rbind_fw(Column *self, Column **cols, int64_t nrows)
{
    size_t elemsize = stype_info[self->stype].elemsize;
    const void *na = stype_info[self->stype].na;
    int col_empty = (self->refcount == 0);

    // Reallocate the column's data buffer
    size_t old_nrows = (size_t) self->nrows;
    size_t old_alloc_size = self->alloc_size;
    size_t new_alloc_size = elemsize * (size_t) nrows;
    dtrealloc(self->data, void, new_alloc_size);
    self->alloc_size = new_alloc_size;
    self->nrows = nrows;
    self->refcount = 1;

    // Copy the data
    void *resptr = col_empty? self->data : add_ptr(self->data, old_alloc_size);
    size_t rows_to_fill = col_empty? old_nrows : 0;
    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype == 0) {
            rows_to_fill += (size_t) col->nrows;
        } else {
            if (rows_to_fill) {
                set_value(resptr, na, elemsize, rows_to_fill);
                resptr = add_ptr(resptr, rows_to_fill * elemsize);
                rows_to_fill = 0;
            }
            if (col->stype != self->stype) {
                Column *newcol = column_cast(col, self->stype);
                if (newcol == NULL) return NULL;
                column_decref(col);
                col = newcol;
            }
            memcpy(resptr, col->data, col->alloc_size);
            resptr = add_ptr(resptr, col->alloc_size);
        }
        column_decref(col);
    }
    if (rows_to_fill) {
        set_value(resptr, na, elemsize, rows_to_fill);
        resptr = add_ptr(resptr, rows_to_fill * elemsize);
    }
    assert(resptr == add_ptr(self->data, new_alloc_size));

    // Done
    return self;
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
static Column* column_rbind_str32(Column *self, Column **cols, int64_t nrows)
{
    assert(self->stype == ST_STRING_I4_VCHAR);
    size_t elemsize = 4;
    int col_empty = (self->refcount == 0);
    self->refcount = 1;

    // Determine the size of the memory to allocate
    size_t old_nrows = (size_t) self->nrows;
    size_t old_offoff = 0;
    size_t new_data_size = 0;     // size of the string data region
    if (!col_empty) {
        old_offoff = (size_t) ((VarcharMeta*) self->meta)->offoff;
        int32_t *offsets = (int32_t*) add_ptr(self->data, old_offoff);
        new_data_size += (size_t) abs(offsets[self->nrows - 1]) - 1;
    }
    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype == 0) continue;
        int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
        int32_t *offsets = (int32_t*) add_ptr(col->data, offoff);
        new_data_size += (size_t) abs(offsets[col->nrows - 1]) - 1;
    }
    size_t new_offsets_size = elemsize * (size_t) nrows;
    size_t padding_size = column_i4s_padding(new_data_size);
    size_t new_offoff = new_data_size + padding_size;
    size_t new_alloc_size = new_offoff + new_offsets_size;

    // Reallocate the column
    assert(new_alloc_size >= self->alloc_size);
    dtrealloc(self->data, void, new_alloc_size);
    self->alloc_size = new_alloc_size;
    self->nrows = nrows;
    int32_t *offsets = (int32_t*) add_ptr(self->data, new_offoff);
    ((VarcharMeta*) self->meta)->offoff = (int64_t) new_offoff;

    // Move the original offsets
    int32_t rows_to_fill = 0;  // how many rows need to be filled with NAs
    int32_t curr_offset = 0;   // Current offset within string data section
    if (col_empty) {
        rows_to_fill += old_nrows;
        offsets[-1] = -1;
    } else {
        memmove(offsets, add_ptr(self->data, old_offoff), old_nrows * 4);
        offsets[-1] = -1;
        curr_offset = abs(offsets[old_nrows - 1]) - 1;
        offsets += old_nrows;
    }

    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype == 0) {
            rows_to_fill += col->nrows;
        } else {
            if (rows_to_fill) {
                const int32_t na = -curr_offset - 1;
                set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
                offsets += rows_to_fill;
                rows_to_fill = 0;
            }
            int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
            int32_t *col_offsets = (int32_t*) add_ptr(col->data, offoff);
            for (int32_t j = 0; j < col->nrows; j++) {
                int32_t off = col_offsets[j];
                *offsets++ = off > 0? off + curr_offset : off - curr_offset;
            }
            size_t data_size = (size_t)(abs(col_offsets[col->nrows - 1]) - 1);
            memcpy(add_ptr(self->data, curr_offset), col->data, data_size);
            curr_offset += data_size;
        }
        column_decref(col);
    }
    if (rows_to_fill) {
        const int32_t na = -curr_offset - 1;
        set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
    }
    if (padding_size) {
        memset(add_ptr(self->data, new_offoff - padding_size), 0xFF,
               padding_size);
    }

    return self;
}



/**
 * Fill the array `ptr` with `count` values `value` (the value is given as a
 * pointer of size `sz`). As a special case, if `value` pointer is NULL, then
 * fill with 0xFF bytes instead.
 * This is used for filling the columns with NAs.
 */
static void set_value(void * restrict ptr, const void * restrict value,
                      size_t sz, size_t count)
{
    if (count == 0) return;
    if (value == NULL) {
        *(unsigned char *)ptr = 0xFF;
        count *= sz;
        sz = 1;
    } else {
        memcpy(ptr, value, sz);
    }
    size_t final_sz = sz * count;
    for (size_t i = sz; i < final_sz; i <<= 1) {
        size_t writesz = i < final_sz - i ? i : final_sz - i;
        memcpy(ptr + i, ptr, writesz);
    }
}
