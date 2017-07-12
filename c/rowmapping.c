#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "myassert.h"
#include "rowmapping.h"
#include "omp.h"
#include "types.h"
#include "utils.h"

dt_static_assert(offsetof(RowMapping, ind32) == offsetof(RowMapping, ind64),
                 "Addresses of RowMapping->ind32 and RowMapping->ind64 must "
                 "coincide");



/**
 * Internal macro to help iterate over a rowmapping. Assumes that macro `CODE`
 * is defined in scope, and substitutes it into the body of each loop. Within
 * the macro, variable `int64_t j` can be used to refer to the source row that
 * was rowmapped, and `int64_t i` is the "destination" index.
 */
#define ITER_ALL {                                                             \
    RowMappingType rmtype = rowmapping->type;                                  \
    int64_t nrows = rowmapping->length;                                        \
    if (rmtype == RM_SLICE) {                                                  \
        int64_t start = rowmapping->slice.start;                               \
        int64_t step = rowmapping->slice.step;                                 \
        for (int64_t i = 0, j = start; i < nrows; i++, j+= step) {             \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else if (rmtype == RM_ARR32) {                                             \
        int32_t *indices = rowmapping->ind32;                                  \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            int64_t j = (int64_t) indices[i];                                  \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else if (rmtype == RM_ARR64) {                                             \
        int64_t *indices = rowmapping->ind64;                                  \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            int64_t j = indices[i];                                            \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else assert(0);                                                            \
}



/**
 * Given an ARR64 RowMapping `rwm` attempt to compactify it (i.e. convert into
 * an ARR32 rowmapping), and return self if successful (the RowMapping object
 * is modified in-place).
 */
RowMapping* rowmapping_compactify(RowMapping *self)
{
    if (self->type != RM_ARR64 || self->max > INT32_MAX ||
        self->length > INT32_MAX) return 0;

    int64_t *src = self->ind64;
    int32_t *res = (int32_t*) src;  // Note: res writes on top of src!
    int32_t len = (int32_t) self->length;
    for (int32_t i = 0; i < len; i++) {
        res[i] = (int32_t) src[i];
    }
    dtrealloc(res, int32_t, len);
    self->type = RM_ARR32;
    self->ind32 = res;
    return self;
}



/**
 * Construct a RowMapping object from triple `(start, count, step)`. The new
 * object will have type `RM_SLICE`.
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
RowMapping* rowmapping_from_slice(int64_t start, int64_t count, int64_t step)
{
    // check that 0 <= start, count, start + (count-1)*step <= INT64_MAX
    if (start < 0 || count < 0 ||
        (count > 1 && step < -(start/(count - 1))) ||
        (count > 1 && step > (INT64_MAX - start)/(count - 1))) return NULL;

    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->type = RM_SLICE;
    res->length = count;
    res->min = !count? 0 : step >= 0? start : start + step * (count - 1);
    res->max = !count? 0 : step >= 0? start + step * (count - 1) : start;
    res->slice.start = start;
    res->slice.step = step;
    return res;
}



/**
 * Construct an "array" `RowMapping` object from a series of triples
 * `(start, count, step)`. The triples are given as 3 separate arrays of starts,
 * of counts and of steps.
 *
 * This will create either an RM_ARR32 or RM_ARR64 object, depending on which
 * one is sufficient to hold all the indices.
 */
RowMapping* rowmapping_from_slicelist(
    int64_t *starts, int64_t *counts, int64_t *steps, int64_t n)
{
    if (n < 0) return NULL;

    // Compute the total number of elements, and the largest index that needs
    // to be stored. Also check for potential overflows / invalid values.
    int64_t count = 0;
    int64_t minidx = INT64_MAX, maxidx = 0;
    for (int64_t i = 0; i < n; i++) {
        int64_t start = starts[i];
        int64_t step = steps[i];
        int64_t len = counts[i];
        if (len == 0) continue;
        if (len < 0 || start < 0 || count + len > INT64_MAX ||
            (len > 1 && step < -(start/(len - 1))) ||
            (len > 1 && step > (INT64_MAX - start)/(len - 1))) return NULL;
        int64_t end = start + step * (len - 1);
        if (start < minidx) minidx = start;
        if (start > maxidx) maxidx = start;
        if (end < minidx) minidx = end;
        if (end > maxidx) maxidx = end;
        count += len;
    }
    if (maxidx == 0) minidx = 0;
    assert(minidx >= 0 && minidx <= maxidx);

    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->length = count;
    res->min = minidx;
    res->max = maxidx;

    if (count <= INT32_MAX && maxidx <= INT32_MAX) {
        int32_t *rows = NULL;
        dtmalloc(rows, int32_t, count);
        int32_t *rowsptr = rows;
        for (int64_t i = 0; i < n; i++) {
            int32_t j = (int32_t) starts[i];
            int32_t l = (int32_t) counts[i];
            int32_t s = (int32_t) steps[i];
            for (int32_t k = 0; k < l; k++, j += s)
                *rowsptr++ = j;
        }
        assert(rowsptr - rows == count);
        res->ind32 = rows;
        res->type = RM_ARR32;
    } else {
        int64_t *rows = NULL;
        dtmalloc(rows, int64_t, count);
        int64_t *rowsptr = rows;
        for (int64_t i = 0; i < n; i++) {
            int64_t j = starts[i];
            int64_t l = counts[i];
            int64_t s = steps[i];
            for (int64_t k = 0; k < l; k++, j += s)
                *rowsptr++ = j;
        }
        assert(rowsptr - rows == count);
        res->ind64 = rows;
        res->type = RM_ARR64;
    }

    return res;
}



/**
 * Construct a `RowMapping` object from a plain list of int64_t/int32_t indices.
 * These functions steal ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 * The RowMapping constructed is always of the type corresponding to the array
 * passed, in particular we do not attempt to compactify an int64_t[] array into
 * int32_t[] even if it were possible.
 */
RowMapping*
rowmapping_from_i32_array(int32_t *array, int64_t n)
{
    if (n < 0 || n > INT32_MAX) return NULL;
    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->type = RM_ARR32;
    res->length = n;
    res->ind32 = array;
    if (n == 0) {
        res->min = 0;
        res->max = 0;
    } else {
        int32_t min = array[0], max = array[0], nn = (int32_t) n;
        for (int32_t i = 1; i < nn; i++) {
            int32_t x = array[i];
            if (x < min) min = x;
            if (x > max) max = x;
        }
        res->min = (int64_t) min;
        res->max = (int64_t) max;
    }
    return res;
}

RowMapping*
rowmapping_from_i64_array(int64_t *array, int64_t n)
{
    if (n < 0) return NULL;
    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->type = RM_ARR64;
    res->length = n;
    res->ind64 = array;
    if (n == 0) {
        res->min = 0;
        res->max = 0;
    } else {
        int64_t min = array[0], max = array[0];
        for (int64_t i = 1; i < n; i++) {
            int64_t x = array[i];
            if (x < min) min = x;
            if (x > max) max = x;
        }
        res->min = min;
        res->max = max;
    }
    return res;
}



/**
 * Construct a `RowMapping` object using a boolean "data" column `col`. The
 * mapping will contain only those rows where the `filter` contains true
 * values.
 * This function will create an RM_ARR32/64 RowMapping, depending on what is
 * minimally required.
 */
RowMapping* rowmapping_from_datacolumn(Column *col, int64_t nrows)
{
    if (col->stype != ST_BOOLEAN_I1) return NULL;

    int8_t *data = col->data;
    int64_t nout = 0;
    int64_t maxrow = 0;
    for (int64_t i = 0; i < nrows; i++)
        if (data[i] == 1) {
            nout++;
            maxrow = i;
        }

    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->length = nout;
    res->max = maxrow;

    if (nout == 0) {
        res->min = 0;
        res->type = RM_ARR32;
        res->ind32 = NULL;
    } else
    if (nout <= INT32_MAX && maxrow <= INT32_MAX) {
        int32_t *out = NULL;
        dtmalloc(out, int32_t, nout);
        int32_t nn = (int32_t) maxrow;
        for (int32_t i = 0, j = 0; i <= nn; i++) {
            if (data[i] == 1)
                out[j++] = i;
        }
        res->min = out[0];
        res->ind32 = out;
        res->type = RM_ARR32;
    } else {
        int64_t *out = NULL;
        dtmalloc(out, int64_t, nout);
        for (int64_t i = 0, j = 0; i <= maxrow; i++) {
            if (data[i] == 1)
                out[j++] = i;
        }
        res->min = out[0];
        res->ind64 = out;
        res->type = RM_ARR64;
    }

    return res;
}



/**
 * Construct a `RowMapping` object using a boolean data column `col` with
 * another RowMapping applied to it.
 * This function is complimentary to `rowmapping_from_datacolumn()`: if you
 * need to construct a RowMapping from a "view" column, then this column can
 * be mapped to a pair of source data column and a rowmapping object.
 */
RowMapping*
rowmapping_from_column_with_rowmapping(Column *col, RowMapping *rowmapping)
{
    if (col->stype != ST_BOOLEAN_I1) return NULL;

    int8_t *data = col->data;
    int64_t nouts = 0;
    int64_t maxrow = 0;
    #define CODE                                                               \
        if (data[j] == 1) {                                                    \
            nouts++;                                                           \
            maxrow = j;                                                        \
        }
    ITER_ALL
    #undef CODE

    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->length = nouts;
    res->max = maxrow;

    if (nouts == 0) {
        res->min = 0;
        res->type = RM_ARR32;
        res->ind32 = NULL;
    } else
    if (nouts <= INT32_MAX && maxrow <= INT32_MAX) {
        int32_t *out; dtmalloc(out, int32_t, nouts);
        int32_t k = 0;
        #define CODE                                                           \
            if (data[j] == 1) {                                                \
                out[k++] = (int32_t) i;                                        \
            }
        ITER_ALL
        #undef CODE
        res->min = out[0];
        res->ind32 = out;
        res->type = RM_ARR32;
    } else {
        int64_t *out; dtmalloc(out, int64_t, nouts);
        int64_t k = 0;
        #define CODE                                                           \
            if (data[j] == 1) {                                                \
                out[k++] = (int64_t) i;                                        \
            }
        ITER_ALL
        #undef CODE
        res->min = out[0];
        res->ind64 = out;
        res->type = RM_ARR64;
    }

    return res;
}



/**
 * Merge two `RowMapping`s, and return the combined `RowMapping`. Specifically,
 * suppose there are objects A, B, C such that the map from A rows onto B is
 * described by the mapping `rwm_ab`, and the map from rows of B onto C is given
 * by `rwm_bc`. Then the "merged" RowMapping will describe how rows of A are
 * mapped onto rows of C.
 * Rowmapping `rwm_ab` may also be NULL, in which case a clone of `rwm_bc` is
 * returned.
 */
RowMapping* rowmapping_merge(RowMapping *rwm_ab, RowMapping *rwm_bc)
{
    if (rwm_bc == NULL) return NULL;

    int64_t n = rwm_bc->length;
    RowMappingType type_bc = rwm_bc->type;
    RowMappingType type_ab = rwm_ab == NULL? 0 : rwm_ab->type;

    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->length = n;
    if (n == 0) {
        res->type = RM_SLICE;
        res->min = 0;
        res->max = 0;
        res->slice.start = 0;
        res->slice.step = 1;
        return res;
    }

    switch (type_bc) {
        case RM_SLICE: {
            int64_t start_bc = rwm_bc->slice.start;
            int64_t step_bc = rwm_bc->slice.step;
            if (rwm_ab == NULL) {
                res->type = RM_SLICE;
                res->min = rwm_bc->min;
                res->max = rwm_bc->max;
                res->slice.start = start_bc;
                res->slice.step = step_bc;
            }
            else if (type_ab == RM_SLICE) {
                // Product of 2 slices is again a slice.
                int64_t start_ab = rwm_ab->slice.start;
                int64_t step_ab = rwm_ab->slice.step;
                int64_t start = start_ab + step_ab * start_bc;
                int64_t step = step_ab * step_bc;
                res->type = RM_SLICE;
                res->slice.start = start;
                res->slice.step = step;
                res->min = step >= 0 ? start : start + step * (n - 1);
                res->max = step >= 0 ? start + step * (n - 1) : start;
            }
            else if (step_bc == 0) {
                // Special case: if `step_bc` is 0, then C just contains the
                // same value repeated `n` times, and hence can be created as
                // a slice even if `rwm_ab` is an "array" rowmapping.
                res->type = RM_SLICE;
                res->slice.step = 0;
                res->slice.start = (type_ab == RM_ARR32)
                    ? (int64_t) rwm_ab->ind32[start_bc]
                    : (int64_t) rwm_ab->ind64[start_bc];
                res->min = res->max = res->slice.start;
            }
            else if (type_ab == RM_ARR32) {
                // if A->B is ARR32, then all indices in B are int32, and thus
                // any valid slice over B will also be ARR32 (except possibly
                // a slice with step_bc = 0 and n > INT32_MAX).
                int32_t *rowsres; dtmalloc(rowsres, int32_t, n);
                int32_t *rowssrc = rwm_ab->ind32;
                int32_t min = INT32_MAX, max = 0;
                for (int64_t i = 0, ic = start_bc; i < n; i++, ic += step_bc) {
                    int32_t x = rowssrc[ic];
                    rowsres[i] = x;
                    if (x < min) min = x;
                    if (x > max) max = x;
                }
                res->type = RM_ARR32;
                res->ind32 = rowsres;
                res->min = (int64_t) min;
                res->max = (int64_t) max;
            }
            else if (type_ab == RM_ARR64) {
                // if A->B is ARR64, then a slice of B may be either ARR64 or
                // ARR32. We'll create the result as ARR64 first, and then
                // attempt to compactify later.
                int64_t *rowsres; dtmalloc(rowsres, int64_t, n);
                int64_t *rowssrc = rwm_ab->ind64;
                int64_t min = INT64_MAX, max = 0;
                for (int64_t i = 0, ic = start_bc; i < n; i++, ic += step_bc) {
                    int64_t x = rowssrc[ic];
                    rowsres[i] = x;
                    if (x < min) min = x;
                    if (x > max) max = x;
                }
                res->type = RM_ARR64;
                res->ind64 = rowsres;
                res->min = min;
                res->max = max;
                rowmapping_compactify(res);
            }
            else assert(0);
        } break;  // case RM_SLICE

        case RM_ARR32:
        case RM_ARR64: {
            size_t elemsize = (type_bc == RM_ARR32)? 4 : 8;
            if (rwm_ab == NULL) {
                res->type = type_bc;
                res->min = rwm_bc->min;
                res->max = rwm_bc->max;
                memcpy(res->ind32, rwm_bc->ind32, elemsize * (size_t)n);
            }
            else if (type_ab == RM_SLICE) {
                int64_t start_ab = rwm_ab->slice.start;
                int64_t step_ab = rwm_ab->slice.step;
                int64_t *rowsres; dtmalloc(rowsres, int64_t, n);
                if (type_bc == RM_ARR32) {
                    int32_t *rows_bc = rwm_bc->ind32;
                    for (int64_t i = 0; i < n; i++) {
                        rowsres[i] = start_ab + rows_bc[i] * step_ab;
                    }
                } else {
                    int64_t *rows_bc = rwm_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        rowsres[i] = start_ab + rows_bc[i] * step_ab;
                    }
                }
                res->type = RM_ARR64;
                res->ind64 = rowsres;
                res->min = start_ab + step_ab * (step_ab >= 0? rwm_bc->min : rwm_bc->max);
                res->max = start_ab + step_ab * (step_ab >= 0? rwm_bc->max : rwm_bc->min);
                rowmapping_compactify(res);
            }
            else if (type_ab == RM_ARR32 && type_bc == RM_ARR32) {
                int32_t *rows_ac; dtmalloc(rows_ac, int32_t, n);
                int32_t *rows_ab = rwm_ab->ind32;
                int32_t *rows_bc = rwm_bc->ind32;
                int32_t min = INT32_MAX, max = 0;
                for (int64_t i = 0; i < n; i++) {
                    int32_t x = rows_ab[rows_bc[i]];
                    rows_ac[i] = x;
                    if (x < min) min = x;
                    if (x > max) max = x;
                }
                res->type = RM_ARR32;
                res->ind32 = rows_ac;
                res->min = min;
                res->max = max;
            }
            else {
                int64_t *rows_ac; dtmalloc(rows_ac, int64_t, n);
                int64_t min = INT64_MAX, max = 0;
                if (type_ab == RM_ARR32 && type_bc == RM_ARR64) {
                    int32_t *rows_ab = rwm_ab->ind32;
                    int64_t *rows_bc = rwm_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                        if (x < min) min = x;
                        if (x > max) max = x;
                    }
                } else
                if (type_ab == RM_ARR64 && type_bc == RM_ARR32) {
                    int64_t *rows_ab = rwm_ab->ind64;
                    int32_t *rows_bc = rwm_bc->ind32;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                        if (x < min) min = x;
                        if (x > max) max = x;
                    }
                } else
                if (type_ab == RM_ARR64 && type_bc == RM_ARR64) {
                    int64_t *rows_ab = rwm_ab->ind64;
                    int64_t *rows_bc = rwm_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                        if (x < min) min = x;
                        if (x > max) max = x;
                    }
                }
                res->type = RM_ARR64;
                res->ind64 = rows_ac;
                res->min = min;
                res->max = max;
                rowmapping_compactify(res);
            }
        } break;  // case RM_ARRXX

        default: assert(0);
    }
    return res;
}



/**
 * Construct a `RowMapping` object using an external filter function. The
 * provided filter function is expected to take a range of rows `row0:row1` and
 * an output buffer, and writes the indices of the selected rows into that
 * buffer. This function then handles assembling that output into a
 * final RowMapping structure, as well as distributing the work load among
 * multiple threads.
 *
 * @param filterfn
 *     Pointer to the filter function with the signature `(row0, row1, out,
 *     &nouts) -> int`. The filter function has to determine which rows in the
 *     range `row0:row1` are to be included, and write their indices into the
 *     array `out`. It should also store in the variable `nouts` the number of
 *     rows selected.
 *
 * @param nrows
 *     Number of rows in the datatable that is being filtered.
 */
RowMapping*
rowmapping_from_filterfn32(rowmapping_filterfn32 *filterfn, int64_t nrows)
{
    if (nrows > INT32_MAX) return NULL;

    // Output buffer, where we will write the indices of selected rows. This
    // buffer is preallocated to the length of the original dataset, and it will
    // be re-alloced to the proper length in the end. The reason we don't want
    // to scale this array dynamically is because it reduces performance (at
    // least some of the reallocs will have to memmove the data, and moreover
    // the realloc has to occur within a critical section, slowing down the
    // team of threads).
    int32_t *out;
    dtmalloc(out, int32_t, nrows);
    // Number of elements that were written (or tentatively written) so far
    // into the array `out`.
    size_t out_length = 0;
    // We divide the range of rows [0:nrows] into `num_chunks` pieces, each
    // (except the very last one) having `rows_per_chunk` rows. Each such piece
    // is a fundamental unit of work for this function: every thread in the team
    // works on a single chunk at a time, and then moves on to the next chunk
    // in the queue.
    int64_t rows_per_chunk = 65536;
    int64_t num_chunks = (nrows + rows_per_chunk - 1) / rows_per_chunk;

    #pragma omp parallel
    {
        // Intermediate buffer where each thread stores the row numbers it found
        // before they are consolidated into the final output buffer.
        int32_t *buf = malloc((size_t)rows_per_chunk * sizeof(int32_t));
        // Number of elements that are currently being held in `buf`.
        int32_t buf_length = 0;
        // Offset (within the output buffer) where this thread needs to save the
        // contents of its temporary buffer `buf`.
        // The algorithm works as follows: first, the thread calls `filterfn` to
        // fill up its buffer `buf`. After `filterfn` finishes, the variable
        // `buf_length` will contain the number of rows that were selected from
        // the current (`i`th) chunk. Those row numbers are stored in `buf`.
        // Then the thread enters the "ordered" section, where it stores the
        // current length of the output buffer into the `out_offset` variable,
        // and increases the `out_offset` as if it already copied the result
        // there. However the actual copying is done outside the "ordered"
        // section so as to block all other threads as little as possible.
        size_t out_offset = 0;

        #pragma omp for ordered schedule(dynamic, 1)
        for (int64_t i = 0; i < num_chunks; i++) {
            if (buf_length) {
                // This clause is conceptually located after the "ordered"
                // section -- however due to a bug in libgOMP the "ordered"
                // section must come last in the loop. So in order to circumvent
                // the bug, this block had to be moved to the front of the loop.
                size_t bufsize = (size_t)buf_length * sizeof(int32_t);
                memcpy(out + out_offset, buf, bufsize);
                buf_length = 0;
            }

            int64_t row0 = i * rows_per_chunk;
            int64_t row1 = min(row0 + rows_per_chunk, nrows);
            (*filterfn)(row0, row1, buf, &buf_length);

            #pragma omp ordered
            {
                out_offset = out_length;
                out_length += (size_t) buf_length;
            }
        }
        // Note: if the underlying array is small, then some threads may have
        // done nothing at all, and their buffers would be empty.
        if (buf_length) {
            size_t bufsize = (size_t)buf_length * sizeof(int32_t);
            memcpy(out + out_offset, buf, bufsize);
        }
        // End of #pragma omp parallel: clean up any temporary variables.
        free(buf);
    }

    // In the end we shrink the output buffer to the size corresponding to the
    // actual number of elements written.
    dtrealloc(out, int32_t, out_length);

    // Create and return the final rowmapping object from the array of int32
    // indices `out`.
    RowMapping *res = NULL;
    dtmalloc(res, RowMapping, 1);
    res->type = RM_ARR32;
    res->length = (int64_t) out_length;
    res->ind32 = out;
    res->min = out_length? out[0] : 0;
    res->max = out_length? out[out_length - 1] : 0;
    return res;
}



RowMapping*
rowmapping_from_filterfn64(rowmapping_filterfn64 *filterfn, int64_t nrows)
{
    (void)filterfn;
    (void)nrows;
    return NULL;
}



/**
 * RowMapping's destructor.
 */
void rowmapping_dealloc(RowMapping *rowmapping) {
    if (rowmapping == NULL) return;
    switch (rowmapping->type) {
        case RM_ARR32: dtfree(rowmapping->ind32); break;
        case RM_ARR64: dtfree(rowmapping->ind64); break;
        default: /* do nothing */ break;
    }
    dtfree(rowmapping);
}
