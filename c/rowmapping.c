#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "rowmapping.h"
#include "py_utils.h"


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
 * Construct a `RowMapping` object from a series of triples `(start, count,
 * step)`. The triples are given as 3 separate arrays.
 */
RowMapping* RowMapping_from_slicelist(int64_t *starts, int64_t *counts,
                                      int64_t *steps, int64_t n)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;

    int64_t count = 0;
    for (int64_t i = 0; i < n; i++) count += counts[i];

    int64_t *rows = malloc(sizeof(int64_t) * (size_t)count);
    int64_t *rowsptr = rows;
    if (rows == NULL) return NULL;
    for (int64_t i = 0; i < n; i++) {
        int64_t j = starts[i];
        int64_t cnt = counts[i];
        int64_t step = steps[i];
        for (int64_t k = 0; k < cnt; k++, j += step)
            *rowsptr++ = j;
    }
    assert(rowsptr - rows == count);

    res->type = RI_ARRAY;
    res->length = count;
    res->indices = rows;
    return res;
}


/**
 * Construct a `RowMapping` object from a plain list of (int64_t) row indices.
 * This function steals ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 */
RowMapping* RowMapping_from_i64_array(int64_t* array, int64_t length)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;
    res->type = RI_ARRAY;
    res->length = length;
    res->indices = array;
    return res;
}


/**
 * Construct a `RowMapping` object from a plain list of (int64_t) row indices.
 * This function steals ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 */
RowMapping* RowMapping_from_i32_array(int32_t* array, int32_t length)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;
    res->type = RI_ARRAY;
    res->length = length;
    res->indices = malloc(8 * (size_t)length);
    if (res->indices == NULL) return NULL;
    for (int32_t i = 0; i < length; i++) {
        res->indices[i] = (int32_t) array[i];
    }
    free(array);
    return res;
}


/**
 * Construct a `RowMapping` object using a `filter` function. The `filter`
 * function takes the datatable as the first argument, an output buffer as the
 * second, and the range of rows to scan as the third + forth. Then it scans
 * the provided range of rows and writes the row indices that it selected into
 * the output buffer. The function then returns the number of rows it selected.
 */
RowMapping* RowMapping_from_filter(DataTable* dt, filter_fn filter)
{
    RowMapping *res = malloc(sizeof(RowMapping));
    if (res == NULL) return NULL;

    int64_t *buf = malloc(sizeof(int64_t) * (size_t)dt->nrows);
    if (buf == NULL) return NULL;
    int64_t n_rows_selected = filter(dt, buf, 0, dt->nrows);
    realloc(buf, sizeof(int64_t) * (size_t)n_rows_selected);

    res->type = RI_ARRAY;
    res->length = n_rows_selected;
    res->indices = buf;
    return res;
}


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
    int64_t nrows = filter->nrows;
    int8_t *data = col->data;

    int64_t *out = TRY(malloc(8 * (size_t)nrows));
    int64_t n_rows_selected = 0;
    if (col->mtype == MT_VIEW) {
        size_t srcindex = ((ViewColumn*)col)->srcindex;
        data = filter->source->columns[srcindex]->data;
        if (filter->rowmapping->type == RI_SLICE) {
            int64_t j = filter->rowmapping->slice.start;
            int64_t step = filter->rowmapping->slice.step;
            for (int64_t i = 0; i < nrows; i++, j += step) {
                if (data[j] == 1)
                    out[n_rows_selected++] = i;
            }
        } else if (filter->rowmapping->type == RI_ARRAY) {
            int64_t *indices = filter->rowmapping->indices;
            for (int64_t i = 0; i < nrows; i++) {
                if (data[indices[i]] == 1)
                    out[n_rows_selected++] = i;
            }
        } else assert(0);
    } else {
        for (int64_t i = 0; i < nrows; i++) {
            if (data[i] == 1)
                out[n_rows_selected++] = i;
        }
    }
    realloc(out, 8 * (size_t)n_rows_selected);

    res->type = RI_ARRAY;
    res->length = n_rows_selected;
    res->indices = out;
    return res;

  fail:
    RowMapping_dealloc(res);
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
    int64_t n = rinew->length;
    res->type = rinew->type;
    res->length = n;

    if (rinew->type == RI_ARRAY) {
        int64_t *rowsnew = rinew->indices;
        res->indices = malloc(sizeof(int64_t) * (size_t)n);
        if (risrc == NULL) {
            memcpy(res->indices, rowsnew, sizeof(int64_t) * (size_t)n);
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
            res->indices = malloc(sizeof(int64_t) * (size_t)n);
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
