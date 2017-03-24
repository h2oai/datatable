#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "rowmapping.h"


/**
 * Construct a `RowMapping` object from triple `(start, count, step)`.
 *
 * Note that we depart from Python's standard of using `(start, end, step)` to
 * denote a slice -- having a `count` gives several advantages:
 *   - computing the "end" is easy and unambiguous: `start + count * step`;
 *     whereas computing "count" from `end` is harder: `(end - start) / step`.
 *   - with explicit `count` the `step` may safely be 0.
 *   - there is no difference in handling positive/negative steps.
 */
RowMapping* RowMapping_from_slice(int64_t start, int64_t count, int64_t step)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;
    res->type = RI_SLICE;
    res->length = count;
    res->slice.start = start;
    res->slice.step = step;
    return res;
}


/**
 * Construct a `RowMapping` object from a plain list of (int64_t) row indices.
 * This function steals ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 */
RowMapping* RowMapping_from_array(int64_t* array, int64_t length)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;
    res->type = RI_ARRAY;
    res->length = length;
    res->indices = array;
    return res;
}


RowMapping* RowMapping_from_filter(DataTable* dt, filter_fn filter)
{
    RowMapping *res = malloc(sizeof(RowMapping));
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
 * RowMapping's destructor.
 */
void RowMapping_dealloc(RowMapping *rowmapping) {
    if (rowmapping == NULL) return;
    if (rowmapping->type == RI_ARRAY) {
        free(rowmapping->indices);
    }
    free(rowmapping);
}
