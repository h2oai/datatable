#include <sys/mman.h>
#include "column.h"
#include "rowmapping.h"
#include "sort.h"
#include "py_utils.h"

// Forward declarations
static void column_dealloc(Column *col);



/**
 * Extract data from column `col` into a new Column object according to the
 * provided `rowmapping`. The new column will have mtype `MT_DATA`. This method
 * can also be used to clone a column.
 */
Column* column_extract(Column *col, RowMapping *rowmapping)
{
    SType stype = col->stype;
    size_t metasize = stype_info[stype].metasize;
    size_t nrows, elemsize;

    // Create the new Column object. Note that `meta` is cloned from the
    // source, which may need adjustment for some cases.
    Column *res = NULL;
    dtmalloc(res, Column, 1);
    res->data = NULL;
    res->alloc_size = 0;
    res->mtype = MT_DATA;
    res->stype = stype;
    res->meta = metasize? TRY(clone(col->meta, metasize)) : NULL;
    res->refcount = 1;

    // If `rowmapping` is not provided, then it's a plain clone.
    if (rowmapping == NULL) {
        res->data = TRY(clone(col->data, col->alloc_size));
        res->alloc_size = col->alloc_size;
        return res;
    }

    nrows = (size_t) rowmapping->length;
    elemsize = stype_info[stype].elemsize;
    if (stype == ST_STRING_FCHAR)
        elemsize = (size_t) ((FixcharMeta*) col->meta)->n;

    // "Slice" rowmapping with step = 1 is a simple subsection of the column
    if (rowmapping->type == RM_SLICE && rowmapping->slice.step == 1) {
        size_t start = (size_t) rowmapping->slice.start;
        switch (stype) {
            #define CASE_IX_VCHAR(etype, abs) {                                \
                size_t offoff = (size_t)((VarcharMeta*) col->meta)->offoff;    \
                etype *offs = (etype*)(col->data + offoff) + start;            \
                etype off0 = start? abs(*(offs - 1)) - 1 : 0;                  \
                etype off1 = start + nrows? abs(*(offs + nrows - 1)) - 1 : 0;  \
                size_t datasize = (size_t)(off1 - off0);                       \
                size_t padding = (8 - (datasize & 7)) & 7;                     \
                size_t offssize = nrows * elemsize;                            \
                offoff = datasize + padding;                                   \
                res->alloc_size = datasize + padding + offssize;               \
                res->data = TRY(malloc(res->alloc_size));                      \
                ((VarcharMeta*) res->meta)->offoff = (int64_t)offoff;          \
                memcpy(res->data, col->data + (size_t)off0, datasize);         \
                memset(res->data + datasize, 0xFF, padding);                   \
                etype *resoffs = (etype*)(res->data + offoff);                 \
                for (size_t i = 0; i < nrows; i++) {                           \
                    resoffs[i] = offs[i] > 0? offs[i] - off0                   \
                                            : offs[i] + off0;                  \
                }                                                              \
            } break;
            case ST_STRING_I4_VCHAR: CASE_IX_VCHAR(int32_t, abs)
            case ST_STRING_I8_VCHAR: CASE_IX_VCHAR(int64_t, llabs)
            #undef CASE_IX_VCHAR

            case ST_STRING_U1_ENUM:
            case ST_STRING_U2_ENUM:
            case ST_STRING_U4_ENUM:
                assert(0);  // not implemented yet
                break;

            default: {
                assert(!stype_info[stype].varwidth);
                size_t alloc_size = nrows * elemsize;
                size_t offset = start * elemsize;
                res->data = TRY(clone(col->data + offset, alloc_size));
                res->alloc_size = alloc_size;
            } break;
        }
        return res;
    }

    // In all other cases we need to iterate through the rowmapping and fetch
    // the required elements manually.
    switch (stype) {
        #define JINIT_SLICE                                                    \
            int64_t start = rowmapping->slice.start;                           \
            int64_t step = rowmapping->slice.step;                             \
            int64_t j = start - step;
        #define JITER_SLICE                                                    \
            j += step;
        #define JINIT_ARR(bits)                                                \
            intXX(bits) *rowindices = rowmapping->ind ## bits;
        #define JITER_ARR(bits)                                                \
            intXX(bits) j = rowindices[i];

        #define CASE_IX_VCHAR_SUB(ctype, abs, JINIT, JITER) {                  \
            size_t offoff = (size_t)((VarcharMeta*) col->meta)->offoff;        \
            ctype *offs = (ctype*)(col->data + offoff);                        \
            size_t datasize = 0;                                               \
            {   JINIT                                                          \
                for (size_t i = 0; i < nrows; i++) {                           \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        datasize += (size_t)(offs[j] - prevoff);               \
                    }                                                          \
                }                                                              \
            }                                                                  \
            size_t padding = (8 - (datasize & 7)) & 7;                         \
            size_t offssize = nrows * elemsize;                                \
            offoff = datasize + padding;                                       \
            res->alloc_size = offoff + offssize;                               \
            res->data = (char*) TRY(malloc(res->alloc_size));                  \
            ((VarcharMeta*) res->meta)->offoff = (int64_t) offoff;             \
            {   JINIT                                                          \
                ctype lastoff = 1;                                             \
                char *dest = res->data;                                        \
                ctype *resoffs = (ctype*)(res->data + offoff);                 \
                for (size_t i = 0; i < nrows; i++) {                           \
                    JITER                                                      \
                    if (offs[j] > 0) {                                         \
                        ctype prevoff = j? abs(offs[j - 1]) : 1;               \
                        size_t len = (size_t)(offs[j] - prevoff);              \
                        if (len) {                                             \
                            memcpy(dest, col->data + prevoff - 1, len);        \
                            dest += len;                                       \
                            lastoff += len;                                    \
                        }                                                      \
                        resoffs[i] = lastoff;                                  \
                    } else                                                     \
                        resoffs[i] = -lastoff;                                 \
                }                                                              \
                memset(dest, 0xFF, padding);                                   \
            }                                                                  \
        }
        #define CASE_IX_VCHAR(ctype, abs)                                      \
            if (rowmapping->type == RM_SLICE)                                  \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_SLICE, JITER_SLICE)        \
            else if (rowmapping->type == RM_ARR32)                             \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(32), JITER_ARR(32))    \
            else if (rowmapping->type == RM_ARR64)                             \
                CASE_IX_VCHAR_SUB(ctype, abs, JINIT_ARR(64), JITER_ARR(64))    \
            break;

        case ST_STRING_I4_VCHAR: CASE_IX_VCHAR(int32_t, abs)
        case ST_STRING_I8_VCHAR: CASE_IX_VCHAR(int64_t, llabs)

        #undef CASE_IX_VCHAR_SUB
        #undef CASE_IX_VCHAR
        #undef JINIT_SLICE
        #undef JINIT_ARRAY
        #undef JITER_SLICE
        #undef JITER_ARRAY

        case ST_STRING_U1_ENUM:
        case ST_STRING_U2_ENUM:
        case ST_STRING_U4_ENUM:
            assert(0);  // not implemented yet
            break;

        default: {
            assert(!stype_info[stype].varwidth);
            size_t alloc_size = nrows * elemsize;
            res->data = TRY(malloc(alloc_size));
            res->alloc_size = alloc_size;
            char *dest = res->data;
            if (rowmapping->type == RM_SLICE) {
                size_t stepsize = (size_t) rowmapping->slice.step * elemsize;
                char *src = col->data + (size_t) rowmapping->slice.start * elemsize;
                for (size_t i = 0; i < nrows; i++) {
                    memcpy(dest, src, elemsize);
                    dest += elemsize;
                    src += stepsize;
                }
            } else
            if (rowmapping->type == RM_ARR32) {
                int32_t *rowindices = rowmapping->ind32;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, col->data + j*elemsize, elemsize);
                    dest += elemsize;
                }
            } else
            if (rowmapping->type == RM_ARR32) {
                int64_t *rowindices = rowmapping->ind64;
                for (size_t i = 0; i < nrows; i++) {
                    size_t j = (size_t) rowindices[i];
                    memcpy(dest, col->data + j*elemsize, elemsize);
                    dest += elemsize;
                }
            }
        } break;
    }
    return res;

  fail:
    free(res->meta);
    free(res->data);
    free(res);
    return NULL;
}



RowMapping* column_sort(Column *col, int64_t nrows)
{
    if (col->stype == ST_INTEGER_I4) {
        int32_t *ordering = NULL;
        sort_i4(col->data, (int32_t)nrows, &ordering);
        return rowmapping_from_i32_array(ordering, (int32_t)nrows);
    }
    return rowmapping_from_slice(0, nrows, 1);
}



/**
 * Increase reference count on column `col`. This function should be called
 * when a new long-term copy to the column is created (for example if the column
 * is referenced from several data tables).
 * Here `col` can also be NULL, in which case this function does nothing.
 */
void column_incref(Column *col)
{
    if (col == NULL) return;
    col->refcount++;
}



/**
 * Decrease reference count on column `col`. Call this function when disposing
 * of a previously held pointer to a column. If the internal reference counter
 * reaches 0, the column's data will be garbage-collected.
 * Here `col` can also be NULL, in which case this function does nothing.
 */
void column_decref(Column *col)
{
    if (col == NULL) return;
    col->refcount--;
    if (col->refcount <= 0) {
        column_dealloc(col);
    }
}



/**
 * Free all memory owned by the column, and then the column itself. This
 * function should not be accessed by the external code: use
 * :func:`column_decref` instead.
 */
static void column_dealloc(Column *col)
{
    if (col->mtype == MT_DATA) {
        dtfree(col->data);
    }
    if (col->mtype == MT_MMAP) {
        munmap(col->data, col->alloc_size);
    }
    dtfree(col->meta);
    dtfree(col);
}
