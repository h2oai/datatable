#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rowindex.h"


/**
 * Construct a `RowIndex` object from triple `(start, count, step)`.
 *
 * Note that we depart from Python's standard of using `(start, end, step)` to
 * denote a slice -- having a `count` gives several advantages:
 *   - computing the "end" is easy and unambiguous: `start + count * step`;
 *     whereas computing "count" from `end` is harder: `(end - start) / step`.
 *   - with explicit `count` the `step` may safely be 0.
 *   - there is no difference in handling positive/negative steps.
 */
RowIndex* RowIndex_from_slice(int64_t start, int64_t count, int64_t step)
{
    RowIndex *res = malloc(sizeof(RowIndex));
    if (res == NULL) return NULL;
    res->type = RI_SLICE;
    res->length = count;
    res->slice.start = start;
    res->slice.step = step;
    return res;
}


/**
 * Construct a `RowIndex` object from a plain list of (int64_t) row indices.
 * This function steals ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 */
RowIndex* RowIndex_from_array(int64_t* array, int64_t length)
{
    RowIndex *res = malloc(sizeof(RowIndex));
    if (res == NULL) return NULL;
    res->type = RI_ARRAY;
    res->length = length;
    res->indices = array;
    return res;
}


RowIndex* RowIndex_from_filter(DataTable* dt, filter_fn filter)
{
    RowIndex *res = malloc(sizeof(RowIndex));
    if (res == NULL) return NULL;

    int64_t *buf = malloc(sizeof(int64_t) * dt->nrows);
    if (buf == NULL) return NULL;
    int64_t n_rows_selected = filter(dt, buf, 0, dt->nrows);
    realloc(buf, sizeof(int64_t) * n_rows_selected);

    res->type = RI_ARRAY;
    res->length = n_rows_selected;
    res->indices = buf;
    return res;
}


/**
 * Merge two `RowIndex`es, and return the combined `RowIndex`. Specifically,
 * suppose there are objects A, B, C such that the map from A rows onto B is
 * described by the index `risrc`, and the map from rows of B onto C is given
 * by `rinew`. Then the "merged" RowIndex will describe how rows of A are
 * mapped onto rows of C.
 * Row index `risrc` may also be NULL, in which case a clone of `rinew` is
 * returned.
 */
RowIndex* RowIndex_merge(RowIndex *risrc, RowIndex *rinew)
{
    RowIndex *res = malloc(sizeof(RowIndex));
    if (res == NULL || rinew == NULL) return NULL;
    int64_t n = rinew->length;
    res->type = rinew->type;
    res->length = n;

    if (rinew->type == RI_ARRAY) {
        int64_t *rowsnew = rinew->indices;
        res->indices = malloc(sizeof(int64_t) * n);
        if (risrc == NULL) {
            memcpy(res->indices, rowsnew, sizeof(int64_t) * n);
        }
        else if (risrc->type == RI_ARRAY) {
            int64_t *rowssrc = risrc->indices;
            for (int64_t i = 0; i < n; ++i) {
                res->indices[i] = rowssrc[rowsnew[i]];
            }
        }
        else if (risrc->type == RI_SLICE) {
            int64_t startsrc = risrc->slice.start;
            int64_t stepsrc = risrc->slice.step;
            for (int64_t i = 0; i < n; ++i) {
                res->indices[i] = startsrc + rowsnew[i] * stepsrc;
            }
        }
        else assert(0);
    }
    else if (rinew->type == RI_SLICE) {
        int64_t startnew = rinew->slice.start;
        int64_t stepnew = rinew->slice.step;
        if (risrc == NULL) {
            res->slice.start = startnew;
            res->slice.step = stepnew;
        }
        else if (risrc->type == RI_ARRAY) {
            res->type = RI_ARRAY;
            res->indices = malloc(sizeof(int64_t) * n);
            if (res->indices == NULL) return NULL;
            int64_t *rowssrc = risrc->indices;
            for (int64_t i = 0, inew = startnew; i < n; ++i, inew += stepnew) {
                res->indices[i] = rowssrc[inew];
            }
        }
        else if (risrc->type == RI_SLICE) {
            int64_t startsrc = risrc->slice.start;
            int64_t stepsrc = risrc->slice.step;
            res->slice.start = startsrc + stepsrc * startnew;
            res->slice.step = stepnew * stepsrc;
        }
        else assert(0);
    }
    else assert(0);

    return res;
}


/**
 * RowIndex's destructor.
 */
void RowIndex_dealloc(RowIndex *rowindex) {
    if (rowindex == NULL) return;
    if (rowindex->type == RI_ARRAY) {
        free(rowindex->indices);
    }
    free(rowindex);
}
