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


/**
 * Data structure that holds all the variables needed to perform radix sort.
 * This object is passed around between the functions that represent different
 * steps in the sorting process, each function reading/writing some of these
 * parameters. The documentation for each function will describe which specific
 * parameters it writes.
 *
 * x
 *      The main data array, depending on `elemsize` has one of the following
 *      types: `uint8_t*`, `uint16_t*`, `uint32_t*` or `uint64_t*` (). The
 *      array has `n` elements. This array serves as a "sorting key" -- the
 *      final goal is to produce the ordering of values in `x`.
 *      The elements in `x` are always unsigned, and will be sorted accordingly.
 *      In particular, the data must usually be transformed in order to ensure
 *      that it sorts correctly (this is done in `prepare_input` step).
 *      If `x` is NULL, it indicates that an error condition was raised, and
 *      the sorting routine should exit as soon as possible. It is also possible
 *      for `x` to be NULL when `issorted` flag is set.
 *
 * o
 *      Current ordering (row indices) of elements in `x`. This is an array of
 *      size `n` (same as `x`). If present, then this array will be sorted
 *      according to the values `x`. If NULL, then it will be treated as if
 *      `o[j] == j`.
 *
 * n
 *      Number of elements in arrays `x` and `o`.
 *
 * elemsize
 *      Size in bytes of each element in `x`, one of: 1, 2, 4, or 8.
 *
 * issorted
 *      Flag indicating that the input array was found to be already sorted (for
 *      example when it is a constant array). When this flag is set, the sorting
 *      procedure should exit as soon as possible.
 *
 * nsigbits
 *      Number of significant bits in the elements of `x`. This cannot exceed
 *      `8 * elemsize`, but could be less. This value is an assertion that all
 *      elements in `x` are in the range `[0; 2**nsigbits)`. The number of
 *      significant bits cannot be 0.
 *
 * shift
 *      The amount of right-shift to be applied to each item in `x` to get the
 *      radix. That is, radix for element `i` is `(x[i] >> shift)`. The `shift`
 *      can also be zero, indicating that the values themselves are the radixes
 *      (as in counting sort).
 *
 * nradixes
 *      Total number of possible radixes, equal to `1 << (nsigbits - shift)`.
 *
 * nth
 *      The number of threads used by OMP.
 *
 * nchunks, chunklen
 *      These variables describe how the total number of rows, `n`, will be
 *      split into smaller chunks for the parallel computation. In particular,
 *      the range `[0; n)` is divided into `nchunks` sub-ranges each except the
 *      last one having length `chunklen`. The last chunk will have the length
 *      `n - chunklen*(nchunks - 1)`. The number of chunks is >= 1, and
 *      `chunklen` is also positive.
 *
 * histogram
 *      Computed during the `build_histogram` step, this array will contain the
 *      histogram of data values, by chunk and by radix. More specifically, this
 *      is a `size_t*` array which is viewed as a 2D table. The `(i,k)`-th
 *      element of this array (where `i` is the chunk index, and `k` is radix)
 *      is located at index `(i * nradixes + k)` and represents the starting
 *      position within the output array where the elements with radix `k` and
 *      within the chunk `i` should be written. That is,
 *          histogram[i,k] = #{elements in chunks 0..i-1 with radix = k} +
 *                           #{elements in all chunks with radix < k}
 *      After the "reorder_data" step, this histogram is modified to contain
 *      values
 *          histogram[i,k] = #{elements in chunks 0..i with radix = k} +
 *                           #{elements in all chunks with radix < k}
 *
 * next_x
 *      When `shift > 0`, a single pass of the `radix_psort()` function will
 *      sort the data array only partially, and 1 or more extra passes will be
 *      needed to finish sorting. In this case the array `next_x` (of length
 *      `n`) will hold pre-sorted and potentially modified values of the
 *      original data array `x`.
 *      The array `next_x` is filled in the "reorder_data" step. If it was NULL,
 *      the array will be allocated, otherwise its contents will be overwritten.
 *
 * next_o
 *      The reordered array `o` to be carried over to the next step, or to be
 *      returned as the final ordering in the end.
 *
 * next_elemsize
 *      Size in bytes of each element in `next_x`. This cannot be greater than
 *      `elemsize`, however `next_elemsize` can be NULL.
 */
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
    int8_t elemsize;
    int8_t next_elemsize;
    int8_t nsigbits;
    int8_t shift;
    int8_t issorted;
    char _padding[3];
} SortContext;



//==============================================================================
// Forward declarations
//==============================================================================
static int32_t* insert_sort_i4(const int32_t*, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_i1(const int8_t*, int32_t*, int32_t*, int32_t);

typedef void     (*prepare_inp_fn)(const Column*, SortContext*);
typedef int32_t* (*insert_sort_fn)(const void*, int32_t*, int32_t*, int32_t);
static prepare_inp_fn prepare_inp_fns[DT_STYPES_COUNT];
static insert_sort_fn insert_sort_fns[DT_STYPES_COUNT];

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
    static void build_histogram_ ## SFX(SortContext *sc)                       \
    {                                                                          \
        T *x = (T*) sc->x;                                                     \
        int8_t shift = sc->shift;                                              \
        size_t counts_size = sc->nchunks * sc->nradixes;                       \
        if (sc->histogram) {                                                   \
            memset(sc->histogram, 0, counts_size * sizeof(size_t));            \
        } else {                                                               \
            dtcalloc_g(sc->histogram, size_t, counts_size);                    \
        }                                                                      \
        size_t *counts = sc->histogram;                                        \
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
        size_t cumsum = 0;                                                     \
        for (size_t j = 0; j < sc->nradixes; j++) {                            \
            for (size_t o = j; o < counts_size; o += sc->nradixes) {           \
                size_t t = counts[o];                                          \
                counts[o] = cumsum;                                            \
                cumsum += t;                                                   \
            }                                                                  \
        }                                                                      \
        return;                                                                \
        fail:                                                                  \
        sc->x = NULL;                                                          \
    }

TEMPLATE_BUILD_HISTOGRAM(u1, uint8_t)
TEMPLATE_BUILD_HISTOGRAM(u2, uint16_t)
TEMPLATE_BUILD_HISTOGRAM(u4, uint32_t)
TEMPLATE_BUILD_HISTOGRAM(u8, uint64_t)
#undef TEMPLATE_BUILD_HISTOGRAM


static void build_histogram(SortContext *sc)
{
    if (sc->elemsize == 1) build_histogram_u1(sc);
    else if (sc->elemsize == 2) build_histogram_u2(sc);
    else if (sc->elemsize == 4) build_histogram_u4(sc);
    else if (sc->elemsize == 8) build_histogram_u8(sc);
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



TEMPLATE_REORDER_DATA(u4u2, uint32_t, uint32_t)
TEMPLATE_REORDER_DATA_SIMPLE(u1, uint8_t)
TEMPLATE_REORDER_DATA_SIMPLE(u2, uint16_t)
#undef TEMPLATE_REORDER_DATA

static void reorder_data(SortContext *sc)
{
    dtmalloc_g(sc->next_x, void, sc->n * (size_t)sc->next_elemsize);
    dtmalloc_g(sc->next_o, int32_t, sc->n);
    switch (sc->elemsize) {
        case 4: reorder_data_u4u2(sc); break;
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
 * Also compute the desired radix size, as a function of `nsigbits` (number of
 * significant bits in the data column).
 *
 * SortContext inputs:
 *      n, nsigbits
 *
 * SortContext outputs:
 *      nth, nchunks, chunklen, shift, nradixes
 */
static void determine_sorting_parameters(SortContext *sc)
{
    size_t nth = (size_t) omp_get_num_threads();
    size_t nch = nth * 2;
    sc->nth = nth;
    sc->chunklen = maxz(1024, (sc->n + nch - 1) / nch);
    sc->nchunks = (sc->n - 1)/sc->chunklen + 1;

    int8_t nradixbits = sc->nsigbits < 16 ? sc->nsigbits : 16;
    sc->shift = sc->nsigbits - nradixbits;
    sc->nradixes = 1 << nradixbits;
}


/**
 * Sort array `x` of length `n` using radix sort algorithm, and return the
 * resulting ordering in variable `o`.
 */
static int32_t* radix_psort(SortContext *sc)
{
    determine_sorting_parameters(sc);
    build_histogram(sc);
    reorder_data(sc);

    assert((sc->shift > 0) == (sc->next_elemsize > 0));
    if (sc->shift > 0) {
        // Prepare the "next SortContext" variable
        SortContext next_sc = {
            .x = NULL,
            .o = NULL,
            .n = 0,
            .elemsize = sc->next_elemsize,
            .issorted = 0,
            .nsigbits = sc->shift,
            .shift = 0,
            .nradixes = 0,
            .nth = 0,
            .nchunks = 0,
            .chunklen = 0,
            .histogram = sc->histogram,  // reuse the `histogram` buffer
            .next_x = sc->x,  // x is no longer needed: reuse
            .next_o = NULL,
            .next_elemsize = 0,
        };

        size_t nradixes = sc->nradixes;
        size_t next_radixes = 1 << sc->shift;

        // Prepare temporary buffer
        size_t tmpsize = maxz(sc->nth * next_radixes, INSERT_SORT_THRESHOLD);
        int32_t *tmp = NULL;
        dtmalloc(tmp, int32_t, tmpsize);

        // At this point the input array is already partially sorted, and the
        // elements that remain to be sorted are collected into contiguous
        // chunks. For example if `shift` is 2, then `next_x` may be:
        //     na na | 0 2 1 3 1 | 2 | 1 1 3 0 | 3 0 0 | 2 2 2 2 2 2
        // For each distinct radix there is a "range" within `next_x` that
        // contains values corresponding to that radix. The values in `next_x`
        // have their most significant bits already removed, since they are
        // constant within each radix range. The array `next_x` is accompanied
        // by array `next_o` which carries the original row numbers of each
        // value. Once we sort `next_o` by the values of `next_x` within each
        // radix range, our job will be complete.

        // First, determine the sizes of ranges corresponding to each radix that
        // remain to be sorted. Recall that the previous step left us with the
        // `counts` array containing cumulative sizes of these ranges, so all
        // we need is to diff that array.
        // Since the `counts` array won't be needed afterwords, we'll just reuse
        // it to hold the "radix ranges map" records. Each such record contains
        // the size of a particular radix region, and its offset within the
        // output array.
        size_t *rrendoffsets = sc->histogram + (sc->nchunks - 1) * sc->nradixes;
        radix_range *rrmap = NULL;
        dtmalloc(rrmap, radix_range, sc->nradixes);
        for (size_t i = 0; i < sc->nradixes; i++) {
            size_t start = i? rrendoffsets[i-1] : 0;
            size_t end = rrendoffsets[i];
            assert(start <= end);
            rrmap[i].size = end - start;
            rrmap[i].offset = start;
        }

        // Sort the radix ranges in the decreasing size order. This is
        // beneficial because processing large groups first and small groups
        // later reduces the amount of time wasted by threads (for example,
        // suppose there are 2 threads and radix ranges have sizes 1M, 1M, 1M,
        // ..., 10M. Then if the groups are processed in this order, the two
        // threads will first do all 1M chunks finishing simultaneously,
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
            next_sc.x = add_ptr(sc->next_x, off * sc->next_elemsize);
            next_sc.o = sc->next_o + off;
            next_sc.n = rrmap[rri].size;
            radix_psort(&next_sc);
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
                              sc->next_o + off, size, tmp, next_radixes);
            }
        }
        dtfree(tmp);
    }

    // Done. Array `oo` contains the ordering of the input vector `x`.
    // dtfree(sc->x);
    // dtfree(xx);
    // dtfree(sc->histogram);
    if (sc->o) {
        memcpy(sc->o, sc->next_o, sc->n * sizeof(int32_t));
    }
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
