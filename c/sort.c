//------------------------------------------------------------------------------
// Sorting/ordering functions.
//
// All functions defined here treat the input arrays as immutable -- i.e. no
// in-place sorting. Instead, each function creates and returns an "ordering"
// vector. The ordering `o` of an array `x` is such a sequence of integers that
// array
//     [x[o[i]] for i in range(n)]
// is sorted in ascending order. The sortings are stable, and will gather all NA
// values in `x` (if any) at the beginning of the sorted list.
//
// See also:
//     https://github.com/Rdatatable/data.table/src/forder.c, fsort.c
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include <omp.h>
#include "column.h"
#include "myassert.h"
#include "rowindex.h"
#include "sort.h"
#include "types.h"
#include "utils.h"


//==============================================================================
// Forward declarations
//==============================================================================
typedef struct SortContext {
    void    *x;
    int32_t *o;
    void    *next_x;
    int32_t *next_o;
    size_t *histogram;
    size_t n;
    size_t nth;
    size_t nchunks;
    size_t chunklen;
    size_t nradixes;
    int8_t own_x;
    int8_t own_o;
    int8_t elemsize;
    int8_t next_elemsize;
    int8_t nsigbits;
    int8_t shift;
    int8_t issorted;
    char _padding;
} SortContext;

static int32_t* insert_sort_i4(const int32_t*, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_i1(const int8_t*, int32_t*, int32_t*, int32_t);

typedef void     (*prepare_inp_fn)(const Column*, SortContext*);
typedef int32_t* (*insert_sort_fn)(const void*, int32_t*, int32_t*, int32_t);
static prepare_inp_fn prepare_inp_fns[DT_STYPES_COUNT];
static insert_sort_fn insert_sort_fns[DT_STYPES_COUNT];

static void* count_psort_i4(int32_t *restrict x, int32_t *restrict y, size_t n, int32_t *restrict temp, size_t range);
static void* count_sort_i4(int32_t*, int32_t*, size_t, int32_t*, size_t);
static int32_t* radix_psort(SortContext*);

static inline size_t maxz(size_t a, size_t b) { return a < b? b : a; }
static inline size_t minz(size_t a, size_t b) { return a < b? a : b; }

#define INSERT_SORT_THRESHOLD 64

typedef struct radix_range { size_t size, offset; } radix_range;
static int _rrcmp(const void *a, const void *b) {
    const size_t x = *(const size_t*)a;
    const size_t y = *(const size_t*)b;
    return (x < y) - (y < x);
}


//==============================================================================
// Main sorting routine
//==============================================================================

/**
 * Sort the column, and return the ordering as a RowIndex object. This function
 * will choose the most appropriate algorithm for sorting. The data in column
 * `col` will not be modified.
 *
 * The function returns NULL if there is a runtime error (for example an
 * intermediate buffer cannot be allocated).
 */
RowIndex* column_sort(Column *col)
{
    if (col->nrows > INT32_MAX) {
        dterrr("Cannot sort a column with %lld rows", col->nrows);
    }
    int32_t nrows = (int32_t) col->nrows;
    int32_t *ordering = NULL;

    if (nrows <= INSERT_SORT_THRESHOLD) {
        insert_sort_fn sortfn = insert_sort_fns[col->stype];
        if (sortfn) {
            ordering = sortfn(col->data, NULL, NULL, nrows);
        } else {
            dterrr("Insert sort not implemented for column of stype %d",
                   col->stype);
        }
    } else {
        prepare_inp_fn prepfn = prepare_inp_fns[col->stype];
        if (prepfn) {
            SortContext sc;
            memset(&sc, 0, sizeof(SortContext));
            prepfn(col, &sc);
            if (sc.issorted) {
                return rowindex_from_slice(0, nrows, 1);
            }
            ordering = radix_psort(&sc);
        } else {
            dterrr("Radix sort not implemented for column of stype %d",
                   col->stype);
        }
    }
    if (!ordering) return NULL;
    return rowindex_from_i32_array(ordering, nrows);
}



//==============================================================================
// "Prepare input" functions
//==============================================================================

/**
 * Compute min/max of the data column, excluding NAs. The computation is done
 * in parallel. If the column contains NAs only, then `min` will hold value
 * INT_MAX, and `max` will hold value -INT_MAX. In all other cases it is
 * guaranteed that `min <= max`.
 *
 * TODO: replace this with the information from RollupStats eventually.
 */
static void compute_min_max_i4(SortContext *sc, int32_t *min, int32_t *max)
{
    int32_t *x = (int32_t*) sc->x;
    int32_t tmin = INT32_MAX;
    int32_t tmax = -INT32_MAX;
    #pragma omp parallel for schedule(static) \
            reduction(min:tmin) reduction(max:tmax)
    for (size_t j = 0; j < sc->n; j++) {
        int32_t t = x[j];
        if (t == NA_I4);
        else if (t < tmin) tmin = t;
        else if (t > tmax) tmax = t;
    }
    *min = tmin;
    *max = tmax;
}



static void prepare_input_i1(const Column *col, SortContext *sc)
{
    size_t n = (size_t) col->nrows;
    uint8_t *xi = (uint8_t*) col->data;
    uint8_t *xo = NULL;
    dtmalloc_g(xo, uint8_t, n);
    uint8_t una = (uint8_t) NA_I1;

    #pragma omp parallel for schedule(static)
    for (size_t j = 0; j < n; j++) {
        xo[j] = xi[j] - una;
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->own_x = 1;
    sc->elemsize = 1;
    sc->nsigbits = 8;
    return;
    fail:
    sc->x = NULL;
}


static void prepare_input_i2(const Column *col, SortContext *sc)
{
    size_t n = (size_t) col->nrows;
    uint16_t *xi = (uint16_t*) col->data;
    uint16_t *xo = NULL;
    dtmalloc_g(xo, uint16_t, n);
    uint16_t una = (uint16_t) NA_I2;

    #pragma omp parallel for schedule(static)
    for (size_t j = 0; j < n; j++) {
        xo[j] = xi[j] - una;
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->own_x = 1;
    sc->elemsize = 2;
    sc->nsigbits = 16;
    return;
    fail:
    sc->x = NULL;
}


static void prepare_input_i4(const Column *col, SortContext *sc)
{
    sc->x = col->data;
    sc->n = (size_t) col->nrows;

    int32_t min, max;
    compute_min_max_i4(sc, &min, &max);
    if (min >= max) {
        // min > max if the column contains NAs only; min == max if the column
        // is constant.
        sc->issorted = 1;
        return;
    }

    int nsigbits = 32 - (int) nlz((uint32_t)(max - min + 1));
    sc->nsigbits = (int8_t) nsigbits;

    uint32_t umin = (uint32_t) min;
    uint32_t una = (uint32_t) NA_I4;
    uint32_t *ux = (uint32_t*) sc->x;
    if (nsigbits <= 16) {
        uint16_t *xx = NULL;
        dtmalloc_g(xx, uint16_t, sc->n);
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < sc->n; j++) {
            uint32_t t = ux[j];
            xx[j] = t == una? 0 : (uint16_t)(t - umin + 1);
        }
        sc->x = (void*) xx;
        sc->elemsize = 2;
        sc->next_elemsize = 0;
        sc->own_x = 1;
    } else {
        uint32_t *xx = NULL;
        dtmalloc_g(xx, uint32_t, sc->n);
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < sc->n; j++) {
            uint32_t t = ux[j];
            xx[j] = t == una? 0 : t - umin + 1;
        }
        sc->x = (void*) xx;
        sc->elemsize = 4;
        sc->next_elemsize = 4;
        sc->own_x = 1;
    }
    return;

    fail:
    sc->x = NULL;
}



//==============================================================================
// Histogram building functions
//==============================================================================


/**
 * Calculate initial histograms of values in `x`. Specifically, we're
 * creating a `counts` table which has `nchunks` rows and `nradix`
 * columns. Cell `[i,j]` in this table will contain the count of values
 * `x` within the chunk `i` such that the topmost `nradixbits` of `x - min`
 * are equal to `j`.
 *
 * More precisely, we put all `x`s equal to NA into cell 0, while other
 * values into the cell `1 + ((x - min)>>shift)`. The expression `x - min`
 * cannot be computed as-is however: it is possible that it overflows the
 * range of int32_t, in which case the behavior is undefined. However
 * casting both of them to uint32_t first avoids this problem and produces
 * the correct result.
 *
 * Allocate `max(nchunks, 2)` because this array will be reused later.
 */
#define TEMPLATE_BUILD_HISTOGRAM(SFX, T)                                       \
    static size_t* build_histogram_ ## SFX(SortContext *sc)                    \
    {                                                                          \
        T *x = (T*) sc->x;                                                     \
        int8_t shift = sc->shift;                                              \
        size_t *counts = NULL;                                                 \
        dtcalloc(counts, size_t, sc->nchunks * sc->nradixes);                  \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++)                               \
        {                                                                      \
            size_t *restrict cnts = counts + (sc->nradixes * i);               \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            for (size_t j = j0; j < j1; j++) {                                 \
                cnts[x[j] >> shift]++;                                         \
            }                                                                  \
        }                                                                      \
        size_t counts_size = sc->nchunks * sc->nradixes;                       \
        size_t cumsum = 0;                                                     \
        for (size_t j = 0; j < sc->nradixes; j++) {                            \
            for (size_t o = j; o < counts_size; o += sc->nradixes) {           \
                size_t t = counts[o];                                          \
                counts[o] = cumsum;                                            \
                cumsum += t;                                                   \
            }                                                                  \
        }                                                                      \
        return counts;                                                         \
    }

TEMPLATE_BUILD_HISTOGRAM(u1, uint8_t)
TEMPLATE_BUILD_HISTOGRAM(u2, uint16_t)
TEMPLATE_BUILD_HISTOGRAM(u4, uint32_t)
TEMPLATE_BUILD_HISTOGRAM(u8, uint64_t)
#undef TEMPLATE_BUILD_HISTOGRAM


static void build_histogram(SortContext *sc)
{
    sc->histogram = sc->elemsize == 1? build_histogram_u1(sc) :
                    sc->elemsize == 2? build_histogram_u2(sc) :
                    sc->elemsize == 4? build_histogram_u4(sc) :
                    sc->elemsize == 8? build_histogram_u8(sc) : NULL;
}



//==============================================================================
// Data processing step
//==============================================================================

/**
 * Perform the main radix shuffle, filling in arrays `oo` and `xx`. The
 * array `oo` will contain the original row numbers of the values in `x`
 * such that `x[oo]` is sorted with respect to the most significant bits.
 * The `xx` array will contain the sorted elements of `x`, with MSB bits
 * already removed -- we need it mostly for page-efficiency at later stages
 * of the algorithm.
 * During execution of this step we also modify the `counts` array, again,
 * so that by the end of the step each cell in `counts` will contain the
 * offset past the *last* item within that cell. In particular, the last row
 * of the `counts` table will contain end-offsets of output regions
 * corresponding to each radix value.
 * This is the last step that accesses the input array in chunks.
 */
#define TEMPLATE_REORDER_DATA(SFX, TI, TO)                                     \
    static void reorder_data_ ## SFX(SortContext *sc)                          \
    {                                                                          \
        int8_t shift = sc->shift;                                              \
        TI mask = (1 << shift) - 1;                                            \
        TI *xi = (TI*) sc->x;                                                  \
        TO *xo = (TO*) sc->next_x;                                             \
        int32_t *oi = sc->o;                                                   \
        int32_t *oo = sc->next_o;                                              \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++) {                             \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            size_t *restrict tcounts = sc->histogram + (sc->nradixes * i);     \
            for (size_t j = j0; j < j1; j++) {                                 \
                size_t k = tcounts[xi[j] >> shift]++;                          \
                oo[k] = oi? oi[j] : (int32_t) j;                               \
                if (xo) xo[k] = xi[j] & mask;                                  \
            }                                                                  \
        }                                                                      \
        assert(sc->histogram[sc->nchunks * sc->nradixes - 1] == sc->n);        \
    }

#define TEMPLATE_REORDER_DATA_SIMPLE(SFX, TI)                                  \
    static void reorder_data_ ## SFX(SortContext *sc)                          \
    {                                                                          \
        TI *xi = (TI*) sc->x;                                                  \
        int32_t *oi = sc->o;                                                   \
        int32_t *oo = sc->next_o;                                              \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++) {                             \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            size_t *restrict tcounts = sc->histogram + (sc->nradixes * i);     \
            for (size_t j = j0; j < j1; j++) {                                 \
                size_t k = tcounts[xi[j]]++;                                   \
                oo[k] = oi? oi[j] : (int32_t) j;                               \
            }                                                                  \
        }                                                                      \
        assert(sc->histogram[sc->nchunks * sc->nradixes - 1] == sc->n);        \
    }



TEMPLATE_REORDER_DATA(u4u4, uint32_t, uint32_t)
TEMPLATE_REORDER_DATA_SIMPLE(u1, uint8_t)
TEMPLATE_REORDER_DATA_SIMPLE(u2, uint16_t)
#undef TEMPLATE_REORDER_DATA

static void reorder_data(SortContext *sc)
{
    dtmalloc_g(sc->next_x, void, sc->n * (size_t)sc->next_elemsize);
    dtmalloc_g(sc->next_o, int32_t, sc->n);
    switch (sc->elemsize) {
        case 4: reorder_data_u4u4(sc); break;
        case 2: reorder_data_u2(sc); break;
        case 1: reorder_data_u1(sc); break;
        default: printf("elemsize = %d\n", sc->elemsize); assert(0);
    }
    return;
    fail:
    sc->next_x = NULL;
}



//==============================================================================
// Radix sort functions
//==============================================================================

/**
 * Determine how the input should be split into chunks: at least as many
 * chunks as the number of threads, unless the input array is too small
 * and chunks become too small in size. We want to have more than 1 chunk
 * per thread so as to reduce delays caused by uneven execution time among
 * threads, on the other hand too many chunks should be avoided because that
 * would increase the time needed to combine the results from different
 * threads.
 */
static void determine_chunk_sizes(SortContext *sc)
{
    sc->nth = (size_t) omp_get_num_threads();
    sc->nchunks = sc->nth * 2;
    sc->chunklen = maxz(1024, (sc->n + sc->nchunks - 1) / sc->nchunks);
    sc->nchunks = (sc->n - 1)/sc->chunklen + 1;
}


/**
 * Sort array `x` of length `n` using radix sort algorithm, and return the
 * resulting ordering in variable `o`.
 */
static int32_t* radix_psort(SortContext *sc)
{
    // Compute the desired radix size, as a function of `sc->nsigbits` (number
    // of significant bits in the data column).
    //   nradixbits: how many bits to use for the radix
    //   shift: the right-shift needed to leave the radix only
    //   nradixes: how many different radixes there will be.
    int nsigbits = sc->nsigbits;
    int nradixbits = nsigbits < 16 ? nsigbits : 16;
    int shift = nsigbits - nradixbits;
    size_t nradixes = 1 << nradixbits;
    sc->shift = (int8_t) shift;
    sc->nradixes = nradixes;

    determine_chunk_sizes(sc);
    build_histogram(sc);
    reorder_data(sc);

    if (shift > 0) {
        // Prepare temporary buffer
        size_t tmpsize = maxz(sc->nth << shift, INSERT_SORT_THRESHOLD);
        int32_t *tmp = NULL;
        dtmalloc(tmp, int32_t, tmpsize);

        // At this point the input array is already partially sorted, and the
        // elements that remain to be sorted are collected into contiguous
        // chunks. For example if `shift` is 2, then `xx` may look like this:
        //     na na | 0 2 1 3 1 | 2 | 1 1 3 0 | 3 0 0 | 2 2 2 2 2 2
        // For each distinct radix there is a "range" within `xx` that contains
        // values corresponding to that radix. The values in `xx` have their
        // most significant bits already removed, since they are constant within
        // each radix range. The array `xx` is accompanied by array `oo` which
        // carries the original row numbers of each value. Once we sort `oo` by
        // the values of `xx` within each radix range, our job will be complete.

        // First, determine the sizes of ranges corresponding to each radix that
        // remain to be sorted. Recall that the previous step left us with the
        // `counts` array containing cumulative sizes of these ranges, so all
        // we need is to diff that array.
        // Since the `counts` array won't be needed afterwords, we'll just reuse
        // it to hold the "radix ranges map" records. Each such record contains
        // the size of a particular radix region, and its offset within the
        // output array.
        size_t *rrendoffsets = sc->histogram + (sc->nchunks - 1) * nradixes;
        radix_range *rrmap = NULL;
        dtmalloc(rrmap, radix_range, nradixes);
        for (size_t i = 0; i < nradixes; i++) {
            size_t start = i? rrendoffsets[i-1] : 0;
            size_t end = rrendoffsets[i];
            assert(start <= end);
            rrmap[i].size = end - start;
            rrmap[i].offset = start;
        }

        // Sort the radix ranges in the decreasing size order. This is
        // beneficial because processing large groups first and small groups
        // later reduces the amount of times wasted by threads (for example,
        // suppose there are 2 groups and radix ranges have sizes 1M, 1M, 1M,
        // ..., 10M. Then if the groups are processed in this order, then the
        // two threads will first do all 1M chunks finishing simultaneously,
        // then the last thread will be doing 10M chunk while the other thread
        // is idle. Working in the opposite direction, one thread will start
        // with 10M chunk, and the other thread will finish all 1M chunks at
        // the same time, minimizing time wasted).
        qsort(rrmap, nradixes, sizeof(radix_range), _rrcmp);
        assert(rrmap[0].size >= rrmap[nradixes - 1].size);

        // At this point the distribution of radix range sizes may or may not
        // be uniform. If the distribution is uniform (i.e. roughly same number
        // of elements in each range), then the best way to proceed is to let
        // all threads process the ranges in-parallel, each thread working on
        // its own range. However if the distribution is highly skewed (i.e.
        // one or several overly large ranges), then such approach will be
        // suboptimal. In the worst-case scenario we would have single thread
        // processing almost the entire array while other threads are idling.
        // In order to combat such "skew", we first process all "large" radix
        // ranges (they are at the beginning of `rrmap`) one-at-a-time and
        // sorting each of them in a multithreaded way.
        // How big should a radix range be to be considered "large"? If there
        // are `n` elements in the array, and the array is split into `k` ranges
        // then the lower bound for the size of the largest range is `n/k`. In
        // fact, if the largest range's size is exactly `n/k`, then all other
        // ranges are forced to have the same sizes (which is an ideal
        // situation). In practice we deem a range to be "large" if its size is
        // more then `2n/k`.
        size_t rri = 0;
        size_t rrlarge = 2 * sc->n / nradixes;
        while (rrmap[rri].size > rrlarge && rri < nradixes) {
            size_t off = rrmap[rri].offset;
            count_psort_i4(add_ptr(sc->next_x, off*4),
                           sc->next_o + off, rrmap[rri].size,
                           tmp, 1 << shift);
            rri++;
        }

        // Finally iterate over all remaining radix ranges, in-parallel, and
        // sort each of them independently using one of the simpler algorithms:
        // counting sort or insertion sort.
        #pragma omp parallel for num_threads(sc->nth)
        for (size_t i = rri; i < nradixes; i++)
        {
            size_t off = rrmap[i].offset;
            size_t size = rrmap[i].size;
            if (size <= INSERT_SORT_THRESHOLD) {
                insert_sort_i4(add_ptr(sc->next_x, off*4),
                               sc->next_o + off, tmp, (int32_t)size);
            } else {
                count_sort_i4(add_ptr(sc->next_x, off*4),
                              sc->next_o + off, size, tmp, 1 << shift);
            }
        }
        dtfree(tmp);
    }

    // Done. Array `oo` contains the ordering of the input vector `x`.
    if (sc->own_x) dtfree(sc->x);
    // dtfree(xx);
    sc->o = sc->next_o;
    return sc->next_o;
}



//==============================================================================
// Insertion sort functions
//
// All functions here provide the same functionality and have a similar
// signature. Each of these function sorts array `y` according to the values of
// array `x`. Both arrays must have the same length `n`. The caller may also
// provide a temporary buffer `tmp` of size at least `4*n` bytes. The contents
// of this array will be overwritten. Returns the pointer `y`.
//
// For example, if `x` is {5, 2, -1, 7, 2}, then this function will leave `x`
// unmodified but reorder the elements of `y` into {y[2], y[1], y[4], y[0],
// y[3]}.
//
// Pointer `y` can be NULL, in which case it will be assumed that `y[i] == i`.
// Similarly, `tmp` can be NULL too, in which case it will be allocated inside
// the function.
//
// This procedure uses Insert Sort algorithm, which has O(n²) complexity.
// Therefore, it should only be used for small arrays (in particular `n` is
// assumed to fit into an integer.
//
// For the string sorting procedure `insert_sort_s4` the argument `x` is
// replaced with a triple `strdata`, `offs`, `skip`. The first is a pointer to
// a memory buffer containing the string data. The `offs` is an array of offsets
// within `strdata` (each `offs[i]` gives the end of string `i`; the beginning
// of the first string is at offset `offs[-1]`). Finally, parameter `skip`
// instructs to compare the strings starting from that byte.
//
// See also:
//   - https://en.wikipedia.org/wiki/Insertion_sort
//   - datatable/microbench/insertsort
//==============================================================================

#define DECLARE_INSERT_SORT_FN(SFX, T)                                         \
    static int32_t* insert_sort_ ## SFX(                                       \
        const T *restrict x,                                                   \
        int32_t *restrict y,                                                   \
        int32_t *restrict tmp,                                                 \
        int32_t n                                                              \
    ) {                                                                        \
        int own_tmp = 0;                                                       \
        if (tmp == NULL) {                                                     \
            dtmalloc(tmp, int32_t, n);                                         \
            own_tmp = 1;                                                       \
        }                                                                      \
        tmp[0] = 0;                                                            \
        for (int i = 1; i < n; i++) {                                          \
            T xi = x[i];                                                       \
            int j = i;                                                         \
            while (j && xi < x[tmp[j - 1]]) {                                  \
                tmp[j] = tmp[j - 1];                                           \
                j--;                                                           \
            }                                                                  \
            tmp[j] = i;                                                        \
        }                                                                      \
        if (!y) return tmp;                                                    \
        for (int i = 0; i < n; i++) {                                          \
            tmp[i] = y[tmp[i]];                                                \
        }                                                                      \
        memcpy(y, tmp, (size_t)n * sizeof(int32_t));                           \
        if (own_tmp) dtfree(tmp);                                              \
        return y;                                                              \
    }

DECLARE_INSERT_SORT_FN(i1, int8_t)
DECLARE_INSERT_SORT_FN(i2, int16_t)
DECLARE_INSERT_SORT_FN(i4, int32_t)
#undef DECLARE_INSERT_SORT_FN



//==============================================================================
// Counting sorts
//==============================================================================

/**
 * Sort array `y` according to the values of array `x`. Both arrays have same
 * size `n`. Array `x` will be overwritten in the process.
 * It is assumed that array `x` contains values in the range `0 .. range - 1`,
 * and the temporary buffer `tmp` has the size sufficient for `nthreads * range`
 * integers.
 *
 * This procedure uses parallel Counting Sort algorithm.
 */
static void* count_psort_i4(
    int32_t *restrict x,
    int32_t *restrict y,
    size_t n,
    int32_t *restrict tmp,
    size_t range
) {
    int nth = omp_get_num_threads();
    size_t nchunks = (size_t) nth;
    size_t chunklen = (n + nchunks - 1) / nchunks;
    size_t counts_size = nchunks * range;

    // Clear the `tmp` buffer
    memset(tmp, 0, counts_size * sizeof(int32_t));

    // Construct parallel histogram, splitting the input into `nchunks` parts
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        int32_t *restrict myx = x + i * chunklen;
        int32_t *const myxend = x + minz((i + 1) * chunklen, n);
        int32_t *restrict mycounts = tmp + (i * range);
        for (; myx < myxend; myx++) {
            mycounts[*myx]++;
        }
    }

    // Compute cumulative sums of the histogram
    int32_t cumsum = 0;
    for (size_t j = 0; j < range; j++) {
        for (size_t offset = j; offset < counts_size; offset += range) {
            int32_t t = tmp[offset];
            tmp[offset] = cumsum;
            cumsum += t;
        }
    }

    int32_t *yy; dtmalloc(yy, int, n);
    // Reorder the data
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        size_t j0 = i * chunklen,
               j1 = minz(j0 + chunklen, n);
        int32_t *restrict mycounts = tmp + (i * range);
        for (size_t j = j0; j < j1; j++) {
            int32_t k = mycounts[x[j]]++;
            yy[k] = y[j];
        }
    }

    // Produce the output
    memcpy(y, yy, n * sizeof(int32_t));
    dtfree(yy);
    return NULL;
}


static void* count_sort_i4(
    int32_t *restrict x,
    int32_t *restrict y,
    size_t n,
    int32_t *restrict tmp,
    size_t range
) {
    memset(tmp, 0, (range + 1) * sizeof(int32_t));
    int32_t *counts = tmp + 1;
    for (size_t i = 0; i < n; i++) {
        counts[x[i]]++;
    }
    counts = tmp;
    for (size_t i = 0; i < n; i++) {
        int32_t k = counts[x[i]]++;
        x[i] = y[k];
    }
    memcpy(y, x, n * sizeof(int32_t));
    return NULL;
}


void init_sort_functions(void)
{
    for (int i = 0; i < DT_STYPES_COUNT; i++) {
        prepare_inp_fns[i] = NULL;
        insert_sort_fns[i] = NULL;
    }
    prepare_inp_fns[ST_BOOLEAN_I1] = (prepare_inp_fn) &prepare_input_i1;
    prepare_inp_fns[ST_INTEGER_I1] = (prepare_inp_fn) &prepare_input_i1;
    prepare_inp_fns[ST_INTEGER_I2] = (prepare_inp_fn) &prepare_input_i2;
    prepare_inp_fns[ST_INTEGER_I4] = (prepare_inp_fn) &prepare_input_i4;

    insert_sort_fns[ST_BOOLEAN_I1] = (insert_sort_fn) &insert_sort_i1;
    insert_sort_fns[ST_INTEGER_I1] = (insert_sort_fn) &insert_sort_i1;
    insert_sort_fns[ST_INTEGER_I2] = (insert_sort_fn) &insert_sort_i2;
    insert_sort_fns[ST_INTEGER_I4] = (insert_sort_fn) &insert_sort_i4;
}
