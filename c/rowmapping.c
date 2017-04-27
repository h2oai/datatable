#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "rowmapping.h"
#include "types.h"
#include "utils.h"


/**
 * Construct a `RowMapping` object from triple `(start, count, step)`.
 *
 * Note that we depart from Python's standard of using `(start, end, step)` to
 * denote a slice -- having a `count` gives several advantages:
 *   - computing the "end" is easy and unambiguous: `start + count * step`;
 *     whereas computing "count" from `end` is harder: `(end - start) / step`.
 *   - with explicit `count` the `step` may safely be 0.
 *   - there is no difference in handling positive/negative steps.
 *
 * Returns a new `RowMapping` object, or NULL if such object cannot be created.
 */
RowMapping* rowmapping_from_slice(ssize_t start, ssize_t count, ssize_t step)
{
    RowMapping *res = NULL;
    res = TRY(malloc(sizeof(RowMapping)));

    // check that 0 <= start, count, start + count*step <= INTPTR_MAX
    if (start < 0 || count < 0) goto fail;
    if (count > 0 && step < -(start/count)) goto fail;
    if (count > 0 && step > (INTPTR_MAX - start)/count) goto fail;

    res->type = RM_SLICE;
    res->length = count;
    res->slice.start = start;
    res->slice.step = step;
    return res;

  fail:
    free(res);
    return NULL;
}


/**
 * Construct an "array" `RowMapping` object from a series of triples
 * `(start, count, step)`. The triples are given as 3 separate arrays.
 */
RowMapping* rowmapping_from_slicelist(
    ssize_t *starts, ssize_t *counts, ssize_t *steps, ssize_t n)
{
    if (n < 0) return NULL;
    size_t nn = (size_t) n;

    RowMapping *res = NULL;
    res = TRY(malloc(sizeof(RowMapping)));

    // Compute the total number of elements, and the largest index that needs
    // to be stored. Also check for potential overflows / invalid values.
    ssize_t count = 0;
    ssize_t maxidx = 0;
    for (ssize_t i = 0; i < n; i++) {
        ssize_t start = starts[i];
        ssize_t step = steps[i];
        ssize_t len = counts[i];
        if (len == 0) continue;
        if (len < 0 || start < 0) goto fail;
        if (step < -(start/len) || step > (INTPTR_MAX - start)/len) goto fail;
        if (count + len > INTPTR_MAX) goto fail;
        ssize_t end = start + step*(len - 1);
        if (end > maxidx) maxidx = end;
        if (start > maxidx) maxidx = start;
        count += len;
    }

    #define CREATE_ROWMAPPING(ctype, bits)                                     \
        ctype *rows = TRY(malloc(sizeof(ctype) * (size_t)count));              \
        ctype *rowsptr = rows;                                                 \
        for (size_t i = 0; i < nn; i++) {                                      \
            size_t cnt = (size_t) counts[i];                                   \
            ctype j = (ctype) starts[i];                                       \
            ctype step = (ctype) steps[i];                                     \
            for (size_t k = 0; k < cnt; k++, j += step)                        \
                *rowsptr++ = j;                                                \
        }                                                                      \
        assert(rowsptr - rows == count);                                       \
        res->length = count;                                                   \
        res->ind ## bits = rows;                                               \
        res->type = RM_ARR ## bits;

    if (count <= INT32_MAX && maxidx <= INT32_MAX) {
        CREATE_ROWMAPPING(int32_t, 32)
    } else {
        assert(_64BIT_);
        CREATE_ROWMAPPING(int64_t, 64)
    }

    #undef CREATE_ROWMAPPING
    return res;

  fail:
    free(res);
    return NULL;
}


/**
 * Construct a `RowMapping` object from a plain list of int64_t/int32_t row
 * indices.
 * This function steals ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 * The RowMapping constructed is always of the type corresponding to the array
 * passed, in particular we do not attempt to compactify an int64_t[] array into
 * int32_t[] even if it is possible.
 */
#define ROWMAPPING_FROM_IXX_ARRAY(bits)                                        \
    RowMapping*                                                                \
    rowmapping_from_i ## bits ## _array(intXX(bits)* array, ssize_t n)         \
    {                                                                          \
        RowMapping *res = NULL;                                                \
        res = TRY(malloc(sizeof(RowMapping)));                                 \
        if (n < 0) goto fail;                                                  \
        res->type = RM_ARR ## bits;                                            \
        res->length = n;                                                       \
        res->ind ## bits = array;                                              \
        return res;                                                            \
      fail:                                                                    \
        free(res);                                                             \
        return NULL;                                                           \
    }

ROWMAPPING_FROM_IXX_ARRAY(32)
ROWMAPPING_FROM_IXX_ARRAY(64)
#undef ROWMAPPING_FROM_IXX_ARRAY



/**
 * Construct a `RowMapping` object using a boolean column `filter`. The mapping
 * will contain only those rows where the `filter` contains truthful values.
 */
RowMapping* RowMapping_from_column(DataTable *filter)
{
    RowMapping *res = NULL;
    res = TRY(malloc(sizeof(RowMapping)));

    if (filter->ncols != 1) goto fail;
    Column *col = filter->columns[0];
    if (col->stype != ST_BOOLEAN_I1) goto fail;
    ssize_t nrows = filter->nrows;
    int8_t *data = col->data;

    if (col->mtype == MT_VIEW) {
        goto fail;
        /*
        ssize_t n_rows_selected = 0;
        size_t srcindex = ((ViewColumn*)col)->srcindex;
        data = filter->source->columns[srcindex]->data;
        if (filter->rowmapping->type == RM_SLICE) {
            int64_t j = filter->rowmapping->slice.start;
            int64_t step = filter->rowmapping->slice.step;
            for (size_t i = 0; i < nrows; i++, j += step) {
                if (data[j] == 1)
                    out[n_rows_selected++] = (int64_t)i;
            }
        } else if (filter->rowmapping->type == RI_ARRAY) {
            int64_t *indices = filter->rowmapping->indices;
            for (size_t i = 0; i < nrows; i++) {
                if (data[indices[i]] == 1)
                    out[n_rows_selected++] = (int64_t)i;
            }
        } else assert(0);
        */
    } else {
        #define MAKE_ROWMAPPING(ctype, bits)                                   \
            ctype *out = TRY(malloc(sizeof(ctype) * (size_t)nrows));           \
            ssize_t n_rows_selected = 0;                                       \
            for (ssize_t i = 0; i < nrows; i++) {                              \
                if (data[i] == 1)                                              \
                    out[n_rows_selected++] = (ctype) i;                        \
            }                                                                  \
            realloc(out, sizeof(ctype) * (size_t)n_rows_selected);             \
            res->length = n_rows_selected;                                     \
            res->type = RM_ARR ## bits;                                        \
            res->ind ## bits = out;                                            \

        if (nrows <= INT32_MAX) {
            MAKE_ROWMAPPING(int32_t, 32)
        } else {
            MAKE_ROWMAPPING(int64_t, 64)
        }
        #undef MAKE_ROWMAPPING
    }
    return res;

  fail:
    rowmapping_dealloc(res);
    return NULL;
}


/**
 * Merge two `RowMapping`s, and return the combined `RowMapping`. Specifically,
 * suppose there are objects A, B, C such that the map from A rows onto B is
 * described by the index `risrc`, and the map from rows of B onto C is given
 * by `rinew`. Then the "merged" RowMapping will describe how rows of A are
 * mapped onto rows of C.
 * Row index `risrc` may also be NULL, in which case a clone of `rinew` is
 * returned.
 */
RowMapping* RowMapping_merge(RowMapping *risrc, RowMapping *rinew)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL || rinew == NULL) return NULL;
    ssize_t n = rinew->length;
    res->type = rinew->type;
    res->length = n;

    switch (rinew->type) {
        // TODO: properly handle 32/64 bits cases
        #define CASE_RM_ARRXX(ctype, bits) {                                   \
            ctype *rowsnew = rinew->ind ## bits;                               \
            res->ind ## bits = malloc(sizeof(ctype) * (size_t)n);              \
            if (risrc == NULL) {                                               \
                memcpy(res->ind ## bits, rowsnew, sizeof(ctype) * (size_t)n);  \
            }                                                                  \
            else if (risrc->type == RM_ARR32) {                                \
                ctype *rowssrc = risrc->ind32;                                 \
                for (ctype i = 0; i < n; ++i) {                                \
                    res->ind ## bits[i] = rowssrc[rowsnew[i]];                 \
                }                                                              \
            }                                                                  \
            else if (risrc->type == RM_ARR64) {                                \
                ctype *rowssrc = risrc->ind64;                                 \
                for (ctype i = 0; i < n; ++i) {                                \
                    res->ind ## bits[i] = rowssrc[rowsnew[i]];                 \
                }                                                              \
            }                                                                  \
            else if (risrc->type == RM_SLICE) {                                \
                ctype startsrc = risrc->slice.start;                           \
                ctype stepsrc = risrc->slice.step;                             \
                for (ctype i = 0; i < n; ++i) {                                \
                    res->ind ## bits[i] = startsrc + rowsnew[i] * stepsrc;     \
                }                                                              \
            }                                                                  \
            else assert(0);                                                    \
        } break;

        case RM_ARR32: CASE_RM_ARRXX(int32_t, 32)
        case RM_ARR64: CASE_RM_ARRXX(int64_t, 64)
        #undef CASE_RM_ARRXX

        case RM_SLICE: {
            int64_t startnew = rinew->slice.start;
            int64_t stepnew = rinew->slice.step;
            if (risrc == NULL) {
                res->slice.start = startnew;
                res->slice.step = stepnew;
            }
            else if (risrc->type == RM_ARR64) {
                res->type = RM_ARR64;
                res->ind64 = malloc(sizeof(int64_t) * (size_t)n);
                if (res->ind64 == NULL) return NULL;
                int64_t *rowssrc = risrc->ind64;
                for (int64_t i = 0, inew = startnew; i < n; ++i, inew += stepnew) {
                    res->ind64[i] = rowssrc[inew];
                }
            }
            else if (risrc->type == RM_SLICE) {
                int64_t startsrc = risrc->slice.start;
                int64_t stepsrc = risrc->slice.step;
                res->slice.start = startsrc + stepsrc * startnew;
                res->slice.step = stepnew * stepsrc;
            }
            else assert(0);
        } break;

        default: assert(0);
    }
    return res;
}


/**
 * RowMapping's destructor.
 */
void rowmapping_dealloc(RowMapping *rowmapping) {
    if (rowmapping == NULL) return;
    switch (rowmapping->type) {
        case RM_ARR32: free(rowmapping->ind32); break;
        case RM_ARR64: free(rowmapping->ind64); break;
        default: /* do nothing */ break;
    }
    free(rowmapping);
}
