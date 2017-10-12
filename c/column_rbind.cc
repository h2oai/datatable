#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include <stdlib.h>  // abs
#include "column.h"
#include "myassert.h"
#include "types.h"
#include "utils.h"


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
Column* Column::rbind(Column **cols)
{
    // Compute the final number of rows and stype
    int64_t res_nrows = this->nrows;
    int64_t nrows0 = res_nrows;
    SType res_stype = stype();
    for (Column **pcol = cols; *pcol; ++pcol) {
        Column *col = *pcol;
        res_nrows += col->nrows;
        if (res_stype < col->stype()) res_stype = col->stype();
    }
    if (res_stype == ST_VOID) res_stype = ST_BOOLEAN_I1;

    // Create the resulting Column object. It can be either: an empty column
    // filled with NAs; the current column (`self`); a clone of the current
    // column (if it has refcount > 1); or a type-cast of the current column.
    Column *res = NULL;
    bool col_empty = (stype() == ST_VOID);
    if (col_empty) {
        res = Column::new_data_column(res_stype, this->nrows);
    } else if (!mbuf->is_readonly() && stype() == res_stype) {
        // Happy place: current column can be modified in-place.
        res = this;
    } else {
        res = (stype() == res_stype) ? deepcopy() : cast(res_stype);
    }
    if (res == NULL) return NULL;
    assert(res->stype() == res_stype && !res->mbuf->is_readonly() &&
           res->nrows == nrows0);

    // TODO: Temporary Fix. To be resolved in #301
    if (res->stats) res->stats->reset();
    // Use the appropriate strategy to continue appending the columns.
    res = (res_stype == ST_STRING_I4_VCHAR) ? res->rbind_str32(cols, res_nrows, col_empty) :
          (!stype_info[res_stype].varwidth) ? res->rbind_fw(cols, res_nrows, col_empty) : NULL;

    // If everything is fine, then the current column can be safely discarded
    // -- the upstream caller will replace this column with the `res`. However
    // if any error occurred (res == NULL), then `self` will be left intact.
    if (res && res != this) delete this;
    return res;
}



/**
 * Helper method designed specifically to append columns of fixed-width stypes.
 * The current column (`self`) will be modified in-place, and must already be
 * "clean", i.e. have refcount 1 and memory type MT_DATA.
 *
 * The `cols` array has the same meaning as in `column_rbind()`. Then, `nrows`
 * is the final row count for the column being constructed. Lastly, flag
 * `col_empty` indicates that the current column is empty and haven't been
 * filled with NAs yet.
 */
Column* Column::rbind_fw(Column **cols, int64_t new_nrows, int col_empty)
{
    size_t elemsize = stype_info[stype()].elemsize;
    const void *na = stype_info[stype()].na;

    // Reallocate the column's data buffer
    size_t old_nrows = (size_t) nrows;
    size_t old_alloc_size = alloc_size();
    size_t new_alloc_size = elemsize * (size_t) new_nrows;
    mbuf->resize(new_alloc_size);
    nrows = new_nrows;

    // Copy the data
    void *resptr = mbuf->at(col_empty ? 0 : old_alloc_size);
    size_t rows_to_fill = col_empty ? old_nrows : 0;
    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype() == 0) {
            rows_to_fill += (size_t) col->nrows;
        } else {
            if (rows_to_fill) {
                set_value(resptr, na, elemsize, rows_to_fill);
                resptr = add_ptr(resptr, rows_to_fill * elemsize);
                rows_to_fill = 0;
            }
            if (col->stype() != stype()) {
                Column *newcol = col->cast(stype());
                if (newcol == NULL) return NULL;
                delete col;
                col = newcol;
            }
            memcpy(resptr, col->data(), col->alloc_size());
            resptr = add_ptr(resptr, col->alloc_size());
        }
        delete col;
    }
    if (rows_to_fill) {
        set_value(resptr, na, elemsize, rows_to_fill);
        resptr = add_ptr(resptr, rows_to_fill * elemsize);
    }
    assert(resptr == mbuf->at(new_alloc_size));

    // Done
    return this;
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
Column* Column::rbind_str32(Column **cols, int64_t new_nrows, int col_empty)
{
    assert(stype() == ST_STRING_I4_VCHAR);
    size_t elemsize = 4;

    // Determine the size of the memory to allocate
    size_t old_nrows = (size_t) nrows;
    size_t old_offoff = 0;
    size_t new_data_size = 0;     // size of the string data region
    if (!col_empty) {
        old_offoff = (size_t) ((VarcharMeta*) meta)->offoff;
        int32_t *offsets = (int32_t*) mbuf->at(old_offoff);
        new_data_size += (size_t) abs(offsets[nrows - 1]) - 1;
    }
    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype() == 0) continue;
        int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
        int32_t *offsets = (int32_t*) col->mbuf->at(offoff);
        new_data_size += (size_t) abs(offsets[col->nrows - 1]) - 1;
    }
    size_t new_offsets_size = elemsize * (size_t) new_nrows;
    size_t padding_size = Column::i4s_padding(new_data_size);
    size_t new_offoff = new_data_size + padding_size;
    size_t new_alloc_size = new_offoff + new_offsets_size;

    // Reallocate the column
    assert(new_alloc_size >= alloc_size());
    mbuf->resize(new_alloc_size);
    nrows = new_nrows;
    int32_t *offsets = (int32_t*) mbuf->at(new_offoff);
    ((VarcharMeta*) meta)->offoff = (int64_t) new_offoff;

    // Move the original offsets
    int32_t rows_to_fill = 0;  // how many rows need to be filled with NAs
    int32_t curr_offset = 0;   // Current offset within string data section
    if (col_empty) {
        rows_to_fill += old_nrows;
        offsets[-1] = -1;
    } else {
        memmove(offsets, mbuf->at(old_offoff), old_nrows * 4);
        offsets[-1] = -1;
        curr_offset = abs(offsets[old_nrows - 1]) - 1;
        offsets += old_nrows;
    }

    for (Column **pcol = cols; *pcol; pcol++) {
        Column *col = *pcol;
        if (col->stype() == 0) {
            rows_to_fill += col->nrows;
        } else {
            if (rows_to_fill) {
                const int32_t na = -curr_offset - 1;
                set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
                offsets += rows_to_fill;
                rows_to_fill = 0;
            }
            int64_t offoff = ((VarcharMeta*) col->meta)->offoff;
            int32_t *col_offsets = (int32_t*) col->mbuf->at(offoff);
            for (int32_t j = 0; j < col->nrows; j++) {
                int32_t off = col_offsets[j];
                *offsets++ = off > 0? off + curr_offset : off - curr_offset;
            }
            size_t data_size = (size_t)(abs(col_offsets[col->nrows - 1]) - 1);
            memcpy(mbuf->at(curr_offset), col->data(), data_size);
            curr_offset += data_size;
        }
        delete col;
    }
    if (rows_to_fill) {
        const int32_t na = -curr_offset - 1;
        set_value(offsets, &na, elemsize, (size_t)rows_to_fill);
    }
    if (padding_size) {
        memset(mbuf->at(new_offoff - padding_size), 0xFF, padding_size);
    }

    return this;
}
