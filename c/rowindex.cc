#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm> // min
#include "myassert.h"
#include "rowindex.h"
#include "myomp.h"
#include "types.h"
#include "utils.h"
#include "datatable_check.h"

dt_static_assert(offsetof(RowIndex, ind32) == offsetof(RowIndex, ind64),
                 "Addresses of RowIndex->ind32 and RowIndex->ind64 must "
                 "coincide");

/**
 * Internal macro to help iterate over a rowindex. Assumes that macro `CODE`
 * is defined in scope, and substitutes it into the body of each loop. Within
 * the macro, variable `int64_t j` can be used to refer to the source row that
 * was mapped, and `int64_t i` is the "destination" index.
 */
#define ITER_ALL {                                                             \
    RowIndexType ritype = rowindex->type;                                      \
    int64_t nrows = rowindex->length;                                          \
    if (ritype == RI_SLICE) {                                                  \
        int64_t start = rowindex->slice.start;                                 \
        int64_t step = rowindex->slice.step;                                   \
        for (int64_t i = 0, j = start; i < nrows; i++, j+= step) {             \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else if (ritype == RI_ARR32) {                                             \
        int32_t *indices = rowindex->ind32;                                    \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            int64_t j = (int64_t) indices[i];                                  \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else if (ritype == RI_ARR64) {                                             \
        int64_t *indices = rowindex->ind64;                                    \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            int64_t j = indices[i];                                            \
            CODE                                                               \
        }                                                                      \
    }                                                                          \
    else assert(0);                                                            \
}



/**
 * Attempt to convert an ARR64 RowIndex object into the ARR32 format. If such
 * conversion is possible, the object will be modified in-place (regardless of
 * its refcount).
 */
void RowIndex::compactify()
{
    if (type != RI_ARR64 || max > INT32_MAX ||
        length > INT32_MAX) return;

    int64_t *src = ind64;
    int32_t *res = (int32_t*) src;  // Note: res writes on top of src!
    int32_t len = (int32_t) length;
    for (int32_t i = 0; i < len; i++) {
        res[i] = (int32_t) src[i];
    }
    dtrealloc_g(res, int32_t, len);
    type = RI_ARR32;
    ind32 = res;
    fail: {}
}




/**
 * Construct a RowIndex object from triple `(start, count, step)`. The new
 * object will have type `RI_SLICE`.
 *
 * Note that we depart from Python's standard of using `(start, end, step)` to
 * denote a slice -- having a `count` gives several advantages:
 *   - computing the "end" is easy and unambiguous: `start + count * step`;
 *     whereas computing "count" from `end` is harder: `(end - start) / step`.
 *   - with explicit `count` the `step` may safely be 0.
 *   - there is no difference in handling positive/negative steps.
 *
 * Returns a new `RowIndex` object, or NULL if such object cannot be created.
 */
RowIndex::RowIndex(int64_t start, int64_t count, int64_t step) :
    length(count),
    type(RI_SLICE),
    refcount(1)
{
    // check that 0 <= start, count, start + (count-1)*step <= INT64_MAX
    if (start < 0 || count < 0 ||
        (count > 1 && step < -(start/(count - 1))) ||
        (count > 1 && step > (INT64_MAX - start)/(count - 1)))
        throw Error("Invalid RowIndex slice (%lld:%lld:%lld)", start, start + step * count, step);
    slice.start = start;
    slice.step = step;
    min = !count? 0 : step >= 0? start : start + step * (count - 1);
    max = !count? 0 : step >= 0? start + step * (count - 1) : start;
}



/**
 * Construct an "array" `RowIndex` object from a series of triples
 * `(start, count, step)`. The triples are given as 3 separate arrays of starts,
 * of counts and of steps.
 *
 * This will create either an RI_ARR32 or RI_ARR64 object, depending on which
 * one is sufficient to hold all the indices.
 */
RowIndex::RowIndex(
    int64_t *starts, int64_t *counts, int64_t *steps, int64_t n) :
    refcount(1)
{
    if (n < 0) throw Error("Invalid slice array length: %lld", n);

    // Compute the total number of elements, and the largest index that needs
    // to be stored. Also check for potential overflows / invalid values.
    int64_t count = 0;
    int64_t minidx = INT64_MAX, maxidx = 0;
    for (int64_t i = 0; i < n; ++i) {
        int64_t start = starts[i];
        int64_t step = steps[i];
        int64_t len = counts[i];
        if (len == 0) continue;
        if (len < 0 || start < 0 || count + len > INT64_MAX ||
            (len > 1 && step < -(start/(len - 1))) ||
            (len > 1 && step > (INT64_MAX - start)/(len - 1)))
            throw Error("Invalid RowIndex slice (%lld:%lld:%lld)", start, start + step * len, step);
        int64_t end = start + step * (len - 1);
        if (start < minidx) minidx = start;
        if (start > maxidx) maxidx = start;
        if (end < minidx) minidx = end;
        if (end > maxidx) maxidx = end;
        count += len;
    }
    if (maxidx == 0) minidx = 0;
    assert(minidx >= 0 && minidx <= maxidx);

    length = count;
    min = minidx;
    max = maxidx;

    if (count <= INT32_MAX && maxidx <= INT32_MAX) {
        int32_t *rows = (int32_t*) malloc(sizeof(int32_t) * (size_t) count);
        int32_t *rowsptr = rows;
        for (int64_t i = 0; i < n; ++i) {
            int32_t j = (int32_t) starts[i];
            int32_t l = (int32_t) counts[i];
            int32_t s = (int32_t) steps[i];
            for (int32_t k = 0; k < l; k++, j += s)
                *rowsptr++ = j;
        }
        assert(rowsptr - rows == count);
        ind32 = rows;
        type = RI_ARR32;
    } else {
        int64_t *rows = NULL;
        rows = (int64_t*) malloc(sizeof(int64_t) * (size_t) count);
        int64_t *rowsptr = rows;
        for (int64_t i = 0; i < n; i++) {
            int64_t j = starts[i];
            int64_t l = counts[i];
            int64_t s = steps[i];
            for (int64_t k = 0; k < l; k++, j += s)
                *rowsptr++ = j;
        }
        assert(rowsptr - rows == count);
        ind64 = rows;
        type = RI_ARR64;
    }
}

/**
 * Construct a `RowIndex` object from a plain list of int64_t/int32_t indices.
 * These functions steal ownership of the `array` -- the caller should not
 * attempt to free it afterwards.
 * The RowIndex constructed is always of the type corresponding to the array
 * passed, in particular we do not attempt to compactify an int64_t[] array into
 * int32_t[] even if it were possible.
 */
RowIndex::RowIndex(int32_t *array, int64_t n, int issorted) :
    length(n),
    ind32(array),
    type(RI_ARR32),
    refcount(1)
{
    if (n < 0 || n > INT32_MAX) throw Error("Invalid int32 array length: %lld", n);
    if (n == 0) {
        min = 0;
        max = 0;
    } else if (issorted) {
        min = (int64_t) array[0];
        max = (int64_t) array[n - 1];
    } else {
        int32_t tmin = INT32_MAX;
        int32_t tmax = -INT32_MAX;
        #pragma omp parallel for schedule(static) \
                reduction(min:tmin) reduction(max:tmax)
        for (int64_t j = 0; j < n; j++) {
            int32_t t = array[j];
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
        min = (int64_t) tmin;
        max = (int64_t) tmax;
    }
}

RowIndex::RowIndex(int64_t *array, int64_t n, int issorted) :
    length(n),
    ind64(array),
    type(RI_ARR64),
    refcount(1)
{
    if (n < 0) throw Error("Invalid int32 array length: %lld", n);
    if (n == 0) {
        min = 0;
        max = 0;
    } else if (issorted) {
        min = array[0];
        max = array[n - 1];
    } else {
        int64_t tmin = INT64_MAX;
        int64_t tmax = -INT64_MAX;
        #pragma omp parallel for schedule(static) \
                reduction(min:tmin) reduction(max:tmax)
        for (int64_t j = 0; j < n; j++) {
            int64_t t = array[j];
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
        min = tmin;
        max = tmax;
    }
}


/**
 * Construct a `RowIndex` object using a boolean "data" column `col`. The
 * index will contain only those rows where the `filter` contains true
 * values.
 * This function will create an RI_ARR32/64 RowIndex, depending on what is
 * minimally required.
 */
RowIndex* RowIndex::from_boolcolumn(Column *col, int64_t nrows)
{
    if (stype_info[col->stype()].ltype != LT_BOOLEAN)
        throw Error("Column is not of boolean type");
    int8_t *data = (int8_t*) col->data();
    int64_t nout = 0;
    int64_t maxrow = 0;
    for (int64_t i = 0; i < nrows; i++)
        if (data[i] == 1) {
            ++nout;
            maxrow = i;
        }

    if (nout == 0) {
        return new RowIndex((int32_t*) NULL, 0, 1);
    } else
    if (nout <= INT32_MAX && maxrow <= INT32_MAX) {
        int32_t *out = (int32_t*) malloc(sizeof(int32_t) * (size_t) nout);
        int32_t nn = (int32_t) maxrow;
        for (int32_t i = 0, j = 0; i <= nn; i++) {
            if (data[i] == 1)
                out[j++] = i;
        }
        return new RowIndex(out, nout, 1);
    } else {
        int64_t *out = (int64_t*) malloc(sizeof(int64_t) * (size_t) nout);
        for (int64_t i = 0, j = 0; i <= maxrow; i++) {
            if (data[i] == 1)
                out[j++] = i;
        }
        return new RowIndex(out, nout, 1);
    }
}



/**
 * Construct a `RowIndex` object using a data column `col` with another
 * RowIndex applied to it.
 *
 * This function is complimentary to `rowindex_from_datacolumn()`: if you
 * need to construct a RowIndex from a "view" column, then this column can
 * be mapped to a pair of source data column and a rowindex object.
 */
RowIndex* RowIndex::from_column(Column *col)
{
    RowIndex* rowindex = col->rowindex();
    RowIndex* res = NULL;
    switch(stype_info[col->stype()].ltype) {
    case LT_BOOLEAN: {
        int8_t *data = (int8_t*) col->data();
        int64_t nouts = 0;
        int64_t maxrow = 0;
        #define CODE                                                            \
            if (data[j] == 1) {                                                 \
                nouts++;                                                        \
                maxrow = j;                                                     \
            }
        ITER_ALL
        #undef CODE

        if (nouts == 0) {
            res = new RowIndex((int32_t*) NULL, nouts, 1);
        } else
        if (nouts <= INT32_MAX && maxrow <= INT32_MAX) {
            int32_t *out = (int32_t*) malloc(sizeof(int32_t) * (size_t) nouts);
            int32_t k = 0;
            #define CODE                                                        \
                if (data[j] == 1) {                                             \
                    out[k++] = (int32_t) i;                                     \
                }
            ITER_ALL
            #undef CODE
            res = new RowIndex(out, nouts, 1);
        } else {
            int64_t *out = (int64_t*) malloc(sizeof(int64_t) * (size_t) nouts);
            int64_t k = 0;
            #define CODE                                                        \
                if (data[j] == 1) {                                             \
                    out[k++] = (int64_t) i;                                     \
                }
            ITER_ALL
            #undef CODE
            res = new RowIndex(out, nouts, 1);
        }
    } break;

    case LT_INTEGER: {
        Column *newcol = col->shallowcopy();
        newcol->reify();
        res = from_intcolumn(newcol, 1);
        delete newcol;
    } break;

    default:
        throw Error("Column in not of boolean or integer type");
    }
    return res;
}



/**
 * Create a RowIndex object from the provided integer column. The values in
 * this column are interpreted as the indices of the rows to be selected.
 * The `is_temp_column` flag indicates that the `col` object will be deleted
 * at the end of the call, so it's ok for the `rowindex_from_intcolumn` to
 * "steal" its data buffer instead of having to copy it.
 */
RowIndex* RowIndex::from_intcolumn(Column *col, int is_temp_column)
{
    if (stype_info[col->stype()].ltype != LT_INTEGER)
        throw Error("Column is not of integer type");

    RowIndex* res = NULL;
    if (col->stype() == ST_INTEGER_I1 || col->stype() == ST_INTEGER_I2) {
        col = col->cast(ST_INTEGER_I4);
        is_temp_column = 2;
    }

    int64_t nrows = col->nrows;
    if (col->stype() == ST_INTEGER_I8) {
        int64_t *arr64 = NULL;
        if (is_temp_column) {
            arr64 = (int64_t*) col->data();
            // col->data() = NULL;
        } else {
            arr64 = (int64_t*) malloc(sizeof(int64_t) * (size_t) nrows);
            memcpy(arr64, col->data(), (size_t) nrows * sizeof(int64_t));
        }
        res = new RowIndex(arr64, nrows, 0);
        res->compactify();
    } else
    if (col->stype() == ST_INTEGER_I4) {
        int32_t *arr32 = NULL;
        if (is_temp_column) {
            arr32 = (int32_t*) col->data();
            // col->data() = NULL;
        } else {
            arr32 = (int32_t*) malloc(sizeof(int32_t) * (size_t) nrows);
            memcpy(arr32, col->data(), (size_t)nrows * sizeof(int32_t));
        }
        res = new RowIndex(arr32, nrows, 0);
    }

    if (is_temp_column == 2) {
        delete col;
    }
    return res;
}


/**
 * Return a (deep) copy of the provided rowindex. This function is rarely
 * useful, a shallow copy is more appropriate in most cases. See
 * `rowindex_incref()`.
 */
RowIndex::RowIndex(const RowIndex &other) :
    length(other.length),
    min(other.min),
    max(other.max),
    type(other.type),
    refcount(1)
{
    switch(type) {
    case RI_SLICE: {
        slice.start = other.slice.start;
        slice.step = other.slice.step;
    } break;
    case RI_ARR32: {
        ind32 = (int32_t*) malloc(sizeof(int32_t) * (size_t) length);
        memcpy(ind32, other.ind32, sizeof(int32_t) * (size_t) length);
    } break;
    case RI_ARR64: {
        ind64 = (int64_t*) malloc(sizeof(int64_t) * (size_t) length);
        memcpy(ind64, other.ind64, sizeof(int64_t) * (size_t) length);
    } break;
    }
}

RowIndex::RowIndex(RowIndex* other) :
    RowIndex(*other) {}


/**
 * Merge two `RowIndex`es, and return the combined rowindex.
 *
 * Specifically, suppose there are data tables A, B, C such that rows of B are
 * a subset of rows of A, and rows of C are a subset of B's. Let `ri_ab`
 * describe the mapping of A's rows onto B's, and `ri_bc` the mapping from
 * B's rows onto C's. Then the "merged" RowIndex shall describe how the rows of
 * A are mapped onto the rows of C.
 * Rowindex `ri_ab` may also be NULL, in which case a clone of `ri_bc` is
 * returned.
 */
RowIndex* RowIndex::merge(RowIndex *ri_ab, RowIndex *ri_bc)
{
    if (ri_ab == nullptr && ri_bc == nullptr) return nullptr;
    if (ri_ab == nullptr) return new RowIndex(ri_bc);
    if (ri_bc == nullptr) return new RowIndex(ri_ab);

    int64_t n = ri_bc->length;
    RowIndexType type_bc = ri_bc->type;
    RowIndexType type_ab = ri_ab->type;

    if (n == 0) {
        return new RowIndex((int64_t) 0, n, 1);
    }
    RowIndex* res = NULL;
    switch (type_bc) {
        case RI_SLICE: {
            int64_t start_bc = ri_bc->slice.start;
            int64_t step_bc = ri_bc->slice.step;
            if (type_ab == RI_SLICE) {
                // Product of 2 slices is again a slice.
                int64_t start_ab = ri_ab->slice.start;
                int64_t step_ab = ri_ab->slice.step;
                int64_t start = start_ab + step_ab * start_bc;
                int64_t step = step_ab * step_bc;
                res = new RowIndex(start, n, step);
            }
            else if (step_bc == 0) {
                // Special case: if `step_bc` is 0, then C just contains the
                // same value repeated `n` times, and hence can be created as
                // a slice even if `ri_ab` is an "array" rowindex.
                int64_t start = (type_ab == RI_ARR32)
                                ? (int64_t) ri_ab->ind32[start_bc]
                                : (int64_t) ri_ab->ind64[start_bc];
                res =  new RowIndex(start, n, 0);
            }
            else if (type_ab == RI_ARR32) {
                // if A->B is ARR32, then all indices in B are int32, and thus
                // any valid slice over B will also be ARR32 (except possibly
                // a slice with step_bc = 0 and n > INT32_MAX).
                int32_t *rowsres; dtmalloc(rowsres, int32_t, n);
                int32_t *rowssrc = ri_ab->ind32;
                for (int64_t i = 0, ic = start_bc; i < n; i++, ic += step_bc) {
                    int32_t x = rowssrc[ic];
                    rowsres[i] = x;
                }
                res = new RowIndex(rowsres, n, 0);
            }
            else if (type_ab == RI_ARR64) {
                // if A->B is ARR64, then a slice of B may be either ARR64 or
                // ARR32. We'll create the result as ARR64 first, and then
                // attempt to compactify later.
                int64_t *rowsres; dtmalloc(rowsres, int64_t, n);
                int64_t *rowssrc = ri_ab->ind64;
                for (int64_t i = 0, ic = start_bc; i < n; i++, ic += step_bc) {
                    int64_t x = rowssrc[ic];
                    rowsres[i] = x;
                }
                res = new RowIndex(rowsres, n, 0);
                res->compactify();
            }
            else assert(0);
        } break;  // case RI_SLICE

        case RI_ARR32:
        case RI_ARR64: {
            if (type_ab == RI_SLICE) {
                int64_t start_ab = ri_ab->slice.start;
                int64_t step_ab = ri_ab->slice.step;
                int64_t *rowsres = (int64_t*) malloc(sizeof(int64_t) * (size_t) n);
                if (type_bc == RI_ARR32) {
                    int32_t *rows_bc = ri_bc->ind32;
                    for (int64_t i = 0; i < n; i++) {
                        rowsres[i] = start_ab + rows_bc[i] * step_ab;
                    }
                } else {
                    int64_t *rows_bc = ri_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        rowsres[i] = start_ab + rows_bc[i] * step_ab;
                    }
                }
                res = new RowIndex(rowsres, n, 0);
                res->compactify();
            }
            else if (type_ab == RI_ARR32 && type_bc == RI_ARR32) {
                int32_t *rows_ac = (int32_t*) malloc(sizeof(int32_t) * (size_t) n);
                int32_t *rows_ab = ri_ab->ind32;
                int32_t *rows_bc = ri_bc->ind32;
                int32_t min = INT32_MAX, max = 0;
                for (int64_t i = 0; i < n; i++) {
                    int32_t x = rows_ab[rows_bc[i]];
                    rows_ac[i] = x;
                    if (x < min) min = x;
                    if (x > max) max = x;
                }
                res = new RowIndex(rows_ac, n, 0);
            }
            else {
                int64_t *rows_ac = (int64_t*) malloc(sizeof(int64_t) * (size_t) n);
                if (type_ab == RI_ARR32 && type_bc == RI_ARR64) {
                    int32_t *rows_ab = ri_ab->ind32;
                    int64_t *rows_bc = ri_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                    }
                } else
                if (type_ab == RI_ARR64 && type_bc == RI_ARR32) {
                    int64_t *rows_ab = ri_ab->ind64;
                    int32_t *rows_bc = ri_bc->ind32;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                    }
                } else
                if (type_ab == RI_ARR64 && type_bc == RI_ARR64) {
                    int64_t *rows_ab = ri_ab->ind64;
                    int64_t *rows_bc = ri_bc->ind64;
                    for (int64_t i = 0; i < n; i++) {
                        int64_t x = rows_ab[rows_bc[i]];
                        rows_ac[i] = x;
                    }
                }
                res = new RowIndex(rows_ac, n, 0);
                res->compactify();
            }
        } break;  // case RM_ARRXX

        default: assert(0);
    }
    return res;
}


/**
 * Construct a `RowIndex` object using an external filter function. The
 * provided filter function is expected to take a range of rows `row0:row1` and
 * an output buffer, and writes the indices of the selected rows into that
 * buffer. This function then handles assembling that output into a
 * final RowIndex structure, as well as distributing the work load among
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
 *
 * @param issorted
 *     When True indicates that the filter function is guaranteed to produce
 *     row index in sorted order.
 */
RowIndex* RowIndex::from_filterfn32(rowindex_filterfn32 *filterfn, int64_t nrows,
                         int issorted)
{
    if (nrows > INT32_MAX) throw Error("nrows (%lld) exceeds scope of int32", nrows);

    // Output buffer, where we will write the indices of selected rows. This
    // buffer is preallocated to the length of the original dataset, and it will
    // be re-alloced to the proper length in the end. The reason we don't want
    // to scale this array dynamically is because it reduces performance (at
    // least some of the reallocs will have to memmove the data, and moreover
    // the realloc has to occur within a critical section, slowing down the
    // team of threads).
    int32_t *out = (int32_t*) malloc(sizeof(int32_t) * (size_t) nrows);
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
        int32_t *buf = (int32_t*) malloc((size_t)rows_per_chunk * sizeof(int32_t));

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
        for (int64_t i = 0; i < num_chunks; ++i) {
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
            int64_t row1 = std::min(row0 + rows_per_chunk, nrows);
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
    out = (int32_t*) realloc(out, sizeof(int32_t) * out_length);

    // Create and return the final rowindex object from the array of int32
    // indices `out`.
    return new RowIndex(out, (int64_t) out_length, issorted);
}



RowIndex* RowIndex::from_filterfn64(rowindex_filterfn64*, int64_t, int)
{
    throw Error("Not implemented");
}



/**
 * Convert a slice RowIndex into an RI_ARR32/RI_ARR64.
 */
RowIndex* RowIndex::expand()
{
    if (type != RI_SLICE) return new RowIndex(this);

    if (length <= INT32_MAX && max <= INT32_MAX) {
        int32_t n = (int32_t) length;
        int32_t start = (int32_t) slice.start;
        int32_t step = (int32_t) slice.step;
        int32_t *out = (int32_t*) malloc(sizeof(int32_t) * (size_t) n);
        #pragma omp parallel for schedule(static)
        for (int32_t i = 0; i < n; ++i) {
            out[i] = start + i*step;
        }
        return new RowIndex(out, n, 1);
    } else {
        int64_t n = length;
        int64_t start = slice.start;
        int64_t step = slice.step;
        dtdeclmalloc(out, int64_t, n);
        #pragma omp parallel for schedule(static)
        for (int64_t i = 0; i < n; i++) {
            out[i] = start + i*step;
        }
        return new RowIndex(out, n, 1);
    }
}


size_t RowIndex::alloc_size()
{
    size_t sz = sizeof(*this);
    switch (type) {
        case RI_ARR32: sz += (size_t)length * sizeof(int32_t); break;
        case RI_ARR64: sz += (size_t)length * sizeof(int64_t); break;
        default: /* do nothing */ break;
    }
    return sz;
}


RowIndex* RowIndex::shallowcopy()
{
    ++refcount;
    return this;
}


void RowIndex::release()
{
    --refcount;
    if (refcount <= 0) delete this;
}


RowIndex::~RowIndex() {
    switch (type) {
        case RI_ARR32: dtfree(ind32); break;
        case RI_ARR64: dtfree(ind64); break;
        default: /* do nothing */ break;
    }
}


/**
 * See DataTable::verify_integrity for method description
 */

bool RowIndex::verify_integrity(IntegrityCheckContext& icc,
                                const std::string& name) const
{
  int nerrors = icc.n_errors();
  auto end = icc.end();

  // Check that rowindex length is valid
  if (length < 0) {
    icc << name << ".length is negative: " << length << end;
    return false;
  }
  // Check for a positive refcount
  if (refcount <= 0) {
    icc << name << " has a nonpositive refcount: " << refcount << end;
  }

  int64_t maxrow = -INT64_MAX;
  int64_t minrow = INT64_MAX;

  switch (type) {
  case RI_SLICE: {
    int64_t start = slice.start;
    // Check that the starting row is not negative
    if (start < 0) {
      icc << "Starting row of " << name << " is negative: " << slice.start
          << end;
      return false;
    }
    int64_t step = slice.step;
    // Ensure that the last index won't lead to an overflow or a negative value
    if (length > 1) {
      if (step > (INT64_MAX - start) / (length - 1)) {
        icc << "Slice in " << name << " leads to integer overflow: start = "
            << start << ", step = " << step << ", length = " << length << end;
        return false;
      }
      if (step < -start / (length - 1)) {
        icc << "Slice in " << name << " has negative indices: start = " << start
            << ", step = " << step << ", length = " << length << end;
        return false;
      }
    }
    int64_t sliceend = start + step * (length - 1);
    maxrow = step > 0 ? sliceend : start;
    minrow = step > 0 ? start : sliceend;
    break;
  }
  case RI_ARR32: {
    // Check that the rowindex length can be represented as an int32
    if (length > INT32_MAX) {
      icc << name << " with type `RI_ARR32` cannot have a length greater than "
          << "INT32_MAX: length = " << length << end;
    }

    // Check that allocation size is valid
    size_t n_allocd = array_size(ind32, sizeof(int32_t));
    if (n_allocd < static_cast<size_t>(length)) {
      icc << name << " requires a minimum array length of " << length
          << " elements, but only allocated enough space for " << n_allocd
          << end;
      return false;
    }

    // Check that every item in the array is a valid value
    for (int32_t i = 0; i < length; ++i) {
      if (ind32[i] < 0) {
        icc << "Item " << i << " in " << name << " is negative: " << ind32[i]
            << end;
      }
      if (ind32[i] > maxrow) maxrow = static_cast<int64_t>(ind32[i]);
      if (ind32[i] < minrow) minrow = static_cast<int64_t>(ind32[i]);
    }
    break;
  }
  case RI_ARR64: {
    // CHeck that the rowindex length can be represented as an int64
    size_t n_allocd = array_size(ind64, sizeof(int64_t));
    if (n_allocd < static_cast<size_t>(length)) {
      icc << name << " requires a minimum array length of " << length
          << " elements, but only allocated enough space for " << n_allocd
          << end;
      return false;
    }

    // Check that every item in the array is a valid value
    for (int64_t i = 0; i < length; ++i) {
      if (ind64[i] < 0) {
        icc << "Item " << i << " in " << name << " is negative: " << ind64[i]
            << end;
      }
      if (ind64[i] > maxrow) maxrow = ind64[i];
      if (ind64[i] < minrow) minrow = ind64[i];
    }
    break;
  }
  default: {
    icc << "Invalid type for " << name << ": " << type << end;
    return false;
  }
  };

  // Check that the found extrema coincides with the reported extrema
  if (length == 0) minrow = maxrow = 0;
  if (this->min != minrow) {
    icc << "Mistmatch between minimum value reported by " << name << " and "
        << "computed minimum: computed minimum is " << minrow << ", whereas "
        << "RowIndex.min = " << this->min << end;
  }
  if (this->max != maxrow) {
    icc << "Mistmatch between maximum value reported by " << name << " and "
        << "computed maximum: computed maximum is " << maxrow << ", whereas "
        << "RowIndex.max = " << this->max << end;
  }

  return !icc.has_errors(nerrors);
}
