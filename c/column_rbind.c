#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include "column.h"
#include "myassert.h"
#include "types.h"
#include "utils.h"

// Forward declarations
static void set_value(void * restrict ptr, const void * restrict value, size_t sz, size_t count);



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
    size_t elemsize = stype_info[stype].elemsize;
    const void *na = stype_info[stype].na;

    // Create the resulting Column object. It can be either: an empty column
    // filled with NAs; the current column (`self`); a clone of the current
    // column; or a type-cast of the current column.
    Column *res = NULL;
    if (self->stype == 0) {
        res = make_data_column(stype, (size_t) self->nrows);
        set_value(res->data, na, elemsize, (size_t) self->nrows);
    } else if (self->refcount == 1 && self->mtype == MT_DATA &&
               self->stype == stype) {
        // Happy place: current column can be modified in-place.
        res = column_incref(self);
    } else {
        res = (self->stype == stype) ? column_copy(self)
                                     : column_cast(self, stype);
    }
    column_decref(self);
    assert(res && res->stype == stype && res->refcount == 1 &&
           res->mtype == MT_DATA && res->nrows == nrows0);

    // Reallocate the column's data buffer
    size_t old_alloc_size = res->alloc_size;
    size_t new_alloc_size = elemsize * (size_t) nrows;
    dtrealloc(res->data, void, new_alloc_size);
    res->alloc_size = new_alloc_size;
    res->nrows = nrows;

    // Copy the data
    void *resptr = add_ptr(res->data, old_alloc_size);
    size_t rows_to_fill = 0;
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
            memcpy(resptr, col->data, col->alloc_size);
            resptr = add_ptr(resptr, col->alloc_size);
        }
        column_decref(col);
    }
    if (rows_to_fill) {
        set_value(resptr, na, elemsize, rows_to_fill);
        resptr = add_ptr(resptr, rows_to_fill * elemsize);
    }
    assert(resptr == add_ptr(res->data, new_alloc_size));

    // Done
    return res;
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
