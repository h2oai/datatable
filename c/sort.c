//------------------------------------------------------------------------------
// Sorting/ordering functions.
//
// We use stable parallel MSD Radix sort algorithm, with fallback to Insertion
// sort at small `n`s.
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
//      https://en.wikipedia.org/wiki/Radix_sort
//      https://en.wikipedia.org/wiki/Insertion_sort
//      http://stereopsis.com/radix.html
//
// Based on Radix sort implementation in (R)data.table:
//      https://github.com/Rdatatable/data.table/src/forder.c
//      https://github.com/Rdatatable/data.table/src/fsort.c
//
//
// Tuning of algorithms / constants in this file is based on `/microbench/sort`.
// The summary of the results obtained from experimentation on a MacOS laptop
// with 4CPUs and 16GB RAM is the following (here k is the number of significant
// bits in unsigned representation of elements of an array, and n is the length
// of that array):
//
// [uint8_t]
//   k=1:  n<=8  insert0, o/w radix1
//   k=2:  n<=8  insert0, o/w radix2
//   k=3:  n<=8  insert0, o/w radix3
//   k=4:  n<=8  insert0, o/w radix4
//   k=5:  n<=12 insert0, o/w radix5
//
// [int]
// k = 1, 2, 3, 4
//     n <= 8 insert0, otherwise radix-K
// k = 5
//     n <= 12 insert0, otherwise radix-5
// k = 6
//     n <= 16 insert0, otherwise radix-6
// k = 7
//     n <= 16 insert0, n <= 40 radix-4/m, otherwise radix-7
// k = 8
//     n <= 16 insert0, n <= 72 radix-4/m, otherwise radix-8
//
//
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdlib.h>  // abs
#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include "utils/omp.h"
#include "column.h"
#include "utils/assert.h"
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
 *      types: `*`, `uint16_t*`, `uint32_t*` or `uint64_t*` (). The
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
 * strdata
 *      For string columns only, this is the common "character data" memory
 *      buffer for all items in the column.
 *
 * stroffs
 *      For string columns only, this is a pointer to the "offsets" structure
 *      of the column.
 *
 * strstart
 *      For string columns only, this is the position within the string that is
 *      currently being tested.
 *
 * strmore
 *      For string columns only, this is a flag that will be set after
 *      `prepare_data` / `reorder_data` and will indicate whether there are any
 *      more characters in the strings available.
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
 * shift, dx
 *      The parameters of linear transform to be applied to each item in `x` to
 *      obtain the radix. That is, radix for element `i` is
 *          ((x[i] + dx) >> shift)
 *      The `shift` and `dx` can also be zero, indicating that the values
 *      themselves are the radixes (as in counting sort).
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
    uint64_t dx;
    const unsigned char *strdata;
    const int32_t *stroffs;
    size_t strstart;
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
    int8_t strmore;
    char _padding[2];
} SortContext;



//==============================================================================
// Forward declarations
//==============================================================================
typedef void (*prepare_inp_fn)(const Column*, int32_t*, size_t, SortContext*);
typedef int32_t* (*insert_sort_fn)(const void*, int32_t*, int32_t*, int32_t);
static prepare_inp_fn prepare_inp_fns[DT_STYPES_COUNT];
static insert_sort_fn insert_sort_fns[DT_STYPES_COUNT];

static void insert_sort(SortContext*);
static void radix_psort(SortContext*);
static int32_t* insert_sort_s4_noo(const unsigned char*, const int32_t*,
                                   int32_t, int32_t*, int32_t);
static int32_t* insert_sort_s4_o(const unsigned char*, const int32_t*,
                                 int32_t, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_u1(const void*, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_u2(const void*, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_u4(const void*, int32_t*, int32_t*, int32_t);
static int32_t* insert_sort_u8(const void*, int32_t*, int32_t*, int32_t);

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
 * Sort the column, and return its ordering as a RowIndex object. This function
 * will choose the most appropriate algorithm for sorting. The data in column
 * `col` will not be modified.
 *
 * The function returns NULL if there is a runtime error (for example an
 * intermediate buffer cannot be allocated).
 */
RowIndex* Column::sort() const
{
    if (nrows > INT32_MAX) {
        throw ValueError() << "Cannot sort a datatable with "
                           << nrows << " rows";
    }
    if (ri != nullptr && (ri->type == RI_ARR64 ||
                          ri->length > INT32_MAX ||
                          ri->max > INT32_MAX)) {
        throw ValueError() << "Cannot sort a datatable which is based on a "
                              "datatable with >2**31 rows";
    }
    int32_t nrows_ = (int32_t) nrows;
    int32_t *ordering = NULL;
    if (nrows_ <= 1) {  // no need to sort
        return new RowIndex((int64_t) 0, nrows_, 1);
    }
    if (ri != nullptr) {
        if (ri->type == RI_ARR32) {
            dtmalloc(ordering, int32_t, nrows_);
            memcpy(ordering, ri->ind32, (size_t) nrows_ * sizeof(int32_t));
        }
        else if (ri->type == RI_SLICE) {
            RowIndex *ri_ = ri->expand();
            if (ri_ == NULL || ri_->type != RI_ARR32) return NULL;
            ordering = ri_->ind32;
            ri_->ind32 = NULL;
            ri_->release();
        }
    }
    SType stype_ = stype();
    prepare_inp_fn prepfn = prepare_inp_fns[stype_];
    SortContext *sc = NULL;
    dtcalloc(sc, SortContext, 1);

    if (nrows_ <= INSERT_SORT_THRESHOLD) {
        if (stype_ == ST_REAL_F4 || stype_ == ST_REAL_F8 || ri != nullptr) {
            prepfn(this, ordering, (size_t)nrows_, sc);
            insert_sort(sc);
            ordering = sc->o;
            dtfree(sc->x);
        } else if (stype_ == ST_STRING_I4_VCHAR) {
            const unsigned char *strdata = (const unsigned char*) data();
            const int32_t *offs = static_cast<const StringColumn<int32_t>*>(this)->offsets();
            ordering = insert_sort_s4_noo(strdata, offs, 0, NULL, nrows_);
        } else {
            insert_sort_fn sortfn = insert_sort_fns[stype_];
            if (sortfn) {
                ordering = sortfn(data(), NULL, NULL, nrows_);
            }
            else
                throw ValueError() << "Insert sort not implemented for column "
                                   << "of stype " << stype_;
        }
    } else {
        if (prepfn) {
            prepfn(this, ordering, (size_t)nrows_, sc);
            if (sc->issorted) {
                return new RowIndex((int64_t) 0, nrows_, 1);
            }
            if (sc->x != NULL) {
                radix_psort(sc);
            }
            int error_occurred = (sc->x == NULL);
            ordering = sc->o;
            if (sc->x != data()) dtfree(sc->x);
            dtfree(sc->next_x);
            dtfree(sc->next_o);
            dtfree(sc->histogram);
            if (error_occurred) ordering = NULL;
        } else {
            throw ValueError() << "Radix sort not implemented for column "
                               << "of stype " << stype_;
        }
    }
    dtfree(sc);
    if (!ordering) return NULL;
    return new RowIndex(ordering, nrows_, 0);
}


//==============================================================================
// "Prepare input" functions
//
// Preparing input is the first step for the radix sort algorithm. The primary
// goal of this step is to convert the input data from signed/float/string
// representation into an array of unsigned integers.
// If preparing data fails, then `sc->x` will be set to NULL.
//
// SortContext inputs:
//      -
//
// SortContext outputs:
//      x, n, elemsize, nsigbits, next_elemsize, (?)issorted
//
//==============================================================================

/**
 * Compute min/max of the data column, excluding NAs. The computation is done
 * in parallel. If the column contains NAs only, then `min` will hold value
 * INT_MAX, and `max` will hold value -INT_MAX. In all other cases it is
 * guaranteed that `min <= max`.
 *
 * This is a helper function for `prepare_input_i4`.
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
        if (t != NA_I4) {
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
    }
    *min = tmin;
    *max = tmax;
}

static void compute_min_max_i8(SortContext *sc, int64_t *min, int64_t *max)
{
    int64_t *x = (int64_t*) sc->x;
    int64_t tmin = INT64_MAX;
    int64_t tmax = -INT64_MAX;
    #pragma omp parallel for schedule(static) \
            reduction(min:tmin) reduction(max:tmax)
    for (size_t j = 0; j < sc->n; j++) {
        int64_t t = x[j];
        if (t != NA_I8) {
            if (t < tmin) tmin = t;
            if (t > tmax) tmax = t;
        }
    }
    *min = tmin;
    *max = tmax;
}


/**
 * Compare two strings a and b, each given as a pair of offsets `off0` ..
 * `off1` into the common character buffer `strdata`. If `off1` is negative,
 * then that string is an NA string. If `off0 >= off1`, then the string is
 * considered empty.
 * Return 0 if strings are equal, 1 if a < b, or -1 if a > b. An NA string
 * compares equal to another NA string, and less than any non-NA string. An
 * empty string compares greater than NA, but less than any non-empty string.
 */
static int _compare_offstrings(
    const unsigned char *strdata, int32_t off0a, int32_t off1a, int32_t off0b,
    int32_t off1b
) {
    // Handle NAs and empty strings
    if (off1b < 0) return off1a < 0? 0 : -1;
    if (off1a < 0) return 1;
    int32_t lena = off1a - off0a;
    int32_t lenb = off1b - off0b;
    if (lenb <= 0) return lena <= 0? 0 : -1;
    if (lena <= 0) return 1;

    for (int32_t t = 0; t < lena; t++) {
        if (t == lenb) return -1;
        unsigned char ca = strdata[off0a + t];
        unsigned char cb = strdata[off0b + t];
        if (ca == cb) continue;
        return (ca < cb)? 1 : -1;
    }
    return lena == lenb? 0 : 1;
}


/**
 * Boolean columns have only 3 distinct values: -128, 0 and 1. The transform
 * `(x + 0xBF) >> 6` converts these to 0, 2 and 3 respectively, provided that
 * the addition in parentheses is done as addition of unsigned bytes (i.e.
 * modulo 256).
 */
static void prepare_input_b1(const Column *col, int32_t *ordering, size_t n,
                             SortContext *sc)
{
    if (ordering) {
        uint8_t *xi = (uint8_t*) col->data();
        uint8_t *xo = NULL;
        dtmalloc_g(xo, uint8_t, n);
        uint8_t una = (uint8_t) NA_I1;

        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            uint8_t t = xi[ordering[j]];
            xo[j] = t == una? 0 : t + 1;
        }
        sc->nsigbits = 8;
        sc->x = (void*) xo;
        sc->o = ordering;
    } else {
        sc->shift = 6;
        sc->nsigbits = 2;
        sc->dx = 0xBF;
        sc->x = col->data();
        sc->o = NULL;
    }

    sc->n = n;
    sc->elemsize = 1;
    sc->next_elemsize = 0;
    return;
    fail:
    sc->x = NULL;
}


/**
 * For i1/i2 columns we do not attempt to find the actual minimum, and instead
 * simply translate them into unsigned by subtracting the lowest possible
 * integer value. This maps NA to 0, -INT_MAX to 1, ... and INT_MAX to UINT_MAX.
 */
static void
prepare_input_i1(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    uint8_t una = (uint8_t) NA_I1;
    uint8_t *xi = (uint8_t*) col->data();
    uint8_t *xo = NULL;
    dtmalloc_g(xo, uint8_t, n);

    if (ordering) {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            xo[j] = xi[ordering[j]] - una;
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            xo[j] = xi[j] - una;
        }
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->o = ordering;
    sc->elemsize = 1;
    sc->nsigbits = 8;
    return;
    fail:
        sc->x = NULL;
}


static void
prepare_input_i2(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    uint16_t una = (uint16_t) NA_I2;
    uint16_t *xi = (uint16_t*) col->data();
    uint16_t *xo = NULL;
    dtmalloc_g(xo, uint16_t, n);

    if (ordering) {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            xo[j] = xi[ordering[j]] - una;
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            xo[j] = xi[j] - una;
        }
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->o = ordering;
    sc->elemsize = 2;
    sc->nsigbits = 16;
    return;
    fail:
        sc->x = NULL;
}


/**
 * For i4/i8 columns we transform them by mapping NAs to 0, the minimum of `x`s
 * to 1, ... and the maximum of `x`s to `max - min + 1`.
 */
static void
prepare_input_i4(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    sc->x = col->data();
    sc->n = n;
    sc->o = ordering;

    int32_t min, max;
    compute_min_max_i4(sc, &min, &max);
    if (min > max) {  // the column contains NAs only
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
        if (ordering) {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint32_t t = ux[ordering[j]];
                xx[j] = t == una? 0 : (uint16_t)(t - umin + 1);
            }
        } else {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint32_t t = ux[j];
                xx[j] = t == una? 0 : (uint16_t)(t - umin + 1);
            }
        }
        sc->x = (void*) xx;
        sc->elemsize = 2;
        sc->next_elemsize = 0;
    } else {
        uint32_t *xx = NULL;
        dtmalloc_g(xx, uint32_t, sc->n);
        if (ordering) {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint32_t t = ux[ordering[j]];
                xx[j] = t == una? 0 : t - umin + 1;
            }
        } else {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint32_t t = ux[j];
                xx[j] = t == una? 0 : t - umin + 1;
            }
        }
        sc->x = (void*) xx;
        sc->elemsize = 4;
        sc->next_elemsize = 2;
    }
    return;

    fail:
    sc->x = NULL;
}


static void
prepare_input_i8(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    sc->x = col->data();
    sc->n = n;
    sc->o = ordering;

    int64_t min, max;
    compute_min_max_i8(sc, &min, &max);
    if (min > max) {  // the column contains NAs only
        sc->issorted = 1;
        return;
    }

    int nsigbits = 64 - (int) nlz8((uint64_t)(max - min + 1));
    sc->nsigbits = (int8_t) nsigbits;

    uint64_t umin = (uint64_t) min;
    uint64_t una = (uint64_t) NA_I8;
    uint64_t *ux = (uint64_t*) sc->x;
    if (nsigbits > 32) {
        uint64_t *xx = NULL;
        dtmalloc_g(xx, uint64_t, sc->n);
        if (ordering) {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[ordering[j]];
                xx[j] = t == una? 0 : t - umin + 1;
            }
        } else {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[j];
                xx[j] = t == una? 0 : t - umin + 1;
            }
        }
        sc->x = (void*) xx;
        sc->elemsize = 8;
        sc->next_elemsize = nsigbits > 48? 8 : 4;
    }
    else if (nsigbits > 16) {
        uint32_t *xx = NULL;
        dtmalloc_g(xx, uint32_t, sc->n);
        if (ordering) {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[ordering[j]];
                xx[j] = t == una? 0 : (uint32_t)(t - umin + 1);
            }
        } else {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[j];
                xx[j] = t == una? 0 : (uint32_t)(t - umin + 1);
            }
        }
        sc->x = (void*) xx;
        sc->elemsize = 4;
        sc->next_elemsize = 2;
    } else {
        uint16_t *xx = NULL;
        dtmalloc_g(xx, uint16_t, sc->n);
        if (ordering) {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[ordering[j]];
                xx[j] = t == una? 0 : (uint16_t)(t - umin + 1);
            }
        } else {
            #pragma omp parallel for schedule(static)
            for (size_t j = 0; j < sc->n; j++) {
                uint64_t t = ux[j];
                xx[j] = t == una? 0 : (uint16_t)(t - umin + 1);
            }
        }
        sc->x = (void*) xx;
        sc->elemsize = 2;
        sc->next_elemsize = 0;
    }
    return;

    fail:
    sc->x = NULL;
}


/**
 * For float32/64 we need to carefully manipulate the bits in order to present
 * them in the correct order as uint32/64. At bit level, the structure of
 * IEEE754-1985 float is the following (first 1 bit is the sign, next 8 bits
 * are the exponent, last 23 bits represent the significand):
 *      Bits (uint32 value)          Float value
 *      0 00 000000                  +0
 *      0 00 000001 - 0 00 7FFFFF    Denormal numbers (positive)
 *      0 01 000000 - 0 FE 7FFFFF    +1*2^-126 .. +1.7FFFFF*2^+126
 *      0 FF 000000                  +Inf
 *      0 FF 000001 - 0 FF 7FFFFF    NaNs (positive)
 *      1 00 000000                  -0
 *      1 00 000001 - 1 00 7FFFFF    Denormal numbers (negative)
 *      1 01 000000 - 1 FE 7FFFFF    -1*2^-126 .. -1.7FFFFF*2^+126
 *      1 FF 000000                  -Inf
 *      1 FF 000001 - 1 FF 7FFFFF    NaNs (negative)
 * In order to put these values into correct order, we'll do the following
 * transform:
 *      (1) numbers with sign bit = 0 will turn the sign bit on.
 *      (2) numbers with sign bit = 1 will be XORed with 0xFFFFFFFF
 *      (3) all NAs will be converted to 0, and NaNs to 1
 *
 * Float64 is similar: 1 bit for the sign, 11 bits of exponent, and finally
 * 52 bits of the significand.
 *
 * See also:
 *      https://en.wikipedia.org/wiki/Float32
 *      https://en.wikipedia.org/wiki/Float64
 */
static void
prepare_input_f4(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    // The data are actually floats; but casting `col->data` into `uint32_t*` is
    // equivalent to using a union to convert from float into uint32_t
    // representation.
    uint32_t una = (uint32_t) NA_F4_BITS;
    uint32_t *xi = (uint32_t*) col->data();
    uint32_t *xo = NULL;
    dtmalloc_g(xo, uint32_t, n);

    if (ordering) {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            uint32_t t = xi[ordering[j]];
            xo[j] = ((t & 0x7F800000) == 0x7F800000 && (t & 0x7FFFFF) != 0)
                        ? (t != una)
                        : t ^ ((uint32_t)(-(int32_t)(t>>31)) | 0x80000000);
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            uint32_t t = xi[j];
            xo[j] = ((t & 0x7F800000) == 0x7F800000 && (t & 0x7FFFFF) != 0)
                        ? (t != una)
                        : t ^ ((uint32_t)(-(int32_t)(t>>31)) | 0x80000000);
        }
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->o = ordering;
    sc->elemsize = 4;
    sc->nsigbits = 32;
    sc->next_elemsize = 2;
    return;
    fail:
        sc->x = NULL;
}


#define F64SBT 0x8000000000000000ULL
#define F64EXP 0x7FF0000000000000ULL
#define F64SIG 0x000FFFFFFFFFFFFFULL

static void
prepare_input_f8(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    uint64_t una = (uint64_t) NA_F8_BITS;
    uint64_t *xi = (uint64_t*) col->data();
    uint64_t *xo = NULL;
    dtmalloc_g(xo, uint64_t, n);

    if (ordering) {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            uint64_t t = xi[ordering[j]];
            xo[j] = ((t & F64EXP) == F64EXP && (t & F64SIG) != 0)
                        ? (t != una)
                        : t ^ ((uint64_t)(-(int64_t)(t>>63)) | F64SBT);
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (size_t j = 0; j < n; j++) {
            uint64_t t = xi[j];
            xo[j] = ((t & F64EXP) == F64EXP && (t & F64SIG) != 0)
                        ? (t != una)
                        : t ^ ((uint64_t)(-(int64_t)(t>>63)) | F64SBT);
        }
    }

    sc->n = n;
    sc->x = (void*) xo;
    sc->o = ordering;
    sc->elemsize = 8;
    sc->nsigbits = 64;
    sc->next_elemsize = 8;
    return;
    fail:
        sc->x = NULL;
}


/**
 * For strings, we fill array `x` with the values of the first 2 characters in
 * each string. We also set up auxiliary variables `strdata`, `stroffs`,
 * `strstart` and `strmore` in the SortContext.
 *
 * More specifically, for each string item, if it is NA then we map it to 0;
 * if it is an empty string we map it to 1, otherwise we map it to
 * `(ch[0] + 1) * 256 + (ch[1] + 1) + 1`, where `ch[i]` is the i-th character
 * of the string, or -1 if the string's length is less than or equal to `i`.
 * This doesn't overflow because in UTF-8 the largest legal byte is 0xF7.
 */
static void
prepare_input_s4(const Column *col, int32_t *ordering, size_t n,
                 SortContext *sc)
{
    uint8_t *strbuf = (uint8_t*) add_ptr(col->data(), -1);
    int32_t *offs = static_cast<const StringColumn<int32_t>*>(col)->offsets();
    int maxlen = 0;
    uint16_t *xo = NULL;
    dtmalloc_g(xo, uint16_t, n);

    #pragma omp parallel for schedule(static) reduction(max:maxlen)
    for (size_t j = 0; j < n; j++) {
        int32_t offend = offs[j];
        if (offend < 0) {  // NA
            xo[j] = 0;
        } else {
            int32_t offstart = abs(offs[j-1]);
            int32_t len = offend - offstart;
            uint8_t c1 = len > 0? strbuf[offstart] + 1 : 0;
            uint8_t c2 = len > 1? strbuf[offstart+1] + 1 : 0;
            xo[j] = (uint16_t)(1 + (c1 << 8) + c2);
            if (len > maxlen) maxlen = len;
        }
    }

    sc->strdata = (unsigned char*) col->data();
    sc->stroffs = offs;
    sc->strstart = 0;
    sc->strmore = maxlen > 2;
    sc->n = n;
    sc->x = (void*) xo;
    sc->o = ordering;
    sc->elemsize = 2;
    sc->nsigbits = 16;
    sc->next_elemsize = (maxlen > 2) * 2;
    return;
    fail:
        sc->x = NULL;
}



//==============================================================================
// Histogram building functions
//==============================================================================

/**
 * Calculate initial histograms of values in `x`. Specifically, we're creating
 * a `counts` table which has `nchunks` rows and `nradix` columns. Cell `[i,j]`
 * in this table will contain the count of values `x` within the chunk `i` such
 * that the topmost `nradixbits` of `x` are equal to `j`.
 *
 * If the `histogram` pointer in the SortContext is NULL, then the required
 * memory buffer will be allocated; if the `histogram` is not empty, then this
 * memory region will be cleared and then filled in.
 *
 * SortContext inputs:
 *      x, n, shift, nradixes, nth, nchunks, chunklen,
 *
 * SortContext outputs:
 *      histogram
 */
#define TEMPLATE_BUILD_HISTOGRAM(SFX, T)                                       \
    static void build_histogram_ ## SFX(SortContext *sc)                       \
    {                                                                          \
        T *x = (T*) sc->x;                                                     \
        T dx = (T) sc->dx;                                                     \
        int8_t shift = sc->shift;                                              \
        size_t *counts = NULL;                                                 \
        size_t cumsum = 0;                                                     \
        size_t counts_size = sc->nchunks * sc->nradixes;                       \
        if (sc->histogram) {                                                   \
            memset(sc->histogram, 0, counts_size * sizeof(size_t));            \
        } else {                                                               \
            dtcalloc_g(sc->histogram, size_t, counts_size);                    \
        }                                                                      \
        counts = sc->histogram;                                                \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++)                               \
        {                                                                      \
            size_t *__restrict__ cnts = counts + (sc->nradixes * i);           \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            for (size_t j = j0; j < j1; j++) {                                 \
                T t = x[j] + dx;                                               \
                cnts[t >> shift]++;                                            \
            }                                                                  \
        }                                                                      \
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
 * Perform the main radix shuffle, filling in arrays `next_o` and `next_x`. The
 * array `next_o` will contain the original row numbers of the values in `x`
 * such that `x[next_o]` is sorted with respect to the most significant bits.
 * The `next_x` array will contain the sorted elements of `x`, with MSB bits
 * already removed -- we need it mostly for page-efficiency at later stages
 * of the algorithm.
 *
 * During execution of this step we also modify the `histogram` array so that
 * by the end of the step each cell in `histogram` will contain the offset past
 * the *last* item within that cell. In particular, the last row of the
 * `histogram` table will contain end-offsets of output regions corresponding
 * to each radix value.
 *
 * SortContext inputs:
 *      x, o, n, shift, nradixes, histogram, nth, nchunks, chunklen
 *
 * SortContext outputs:
 *      next_x, next_o, histogram
 *
 */
#define TEMPLATE_REORDER_DATA(SFX, TI, TO)                                     \
    static void reorder_data_ ## SFX(SortContext *sc)                          \
    {                                                                          \
        int8_t shift = sc->shift;                                              \
        TI mask = (TI)((1ULL << shift) - 1);                                   \
        TI *xi = (TI*) sc->x;                                                  \
        TI dx = (TI) sc->dx;                                                   \
        TO *xo = (TO*) sc->next_x;                                             \
        int32_t *oi = sc->o;                                                   \
        int32_t *oo = sc->next_o;                                              \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++) {                             \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            size_t *__restrict__ tcounts = sc->histogram + (sc->nradixes * i); \
            for (size_t j = j0; j < j1; j++) {                                 \
                size_t k = tcounts[(xi[j] + dx) >> shift]++;                   \
                oo[k] = oi? oi[j] : (int32_t) j;                               \
                if (xo) xo[k] = (TO)((TI)(xi[j] + dx) & mask);                 \
            }                                                                  \
        }                                                                      \
        assert(sc->histogram[sc->nchunks * sc->nradixes - 1] == sc->n);        \
    }

#define TEMPLATE_REORDER_DATA_SIMPLE(SFX, TI)                                  \
    static void reorder_data_ ## SFX(SortContext *sc)                          \
    {                                                                          \
        int8_t shift = sc->shift;                                              \
        TI *xi = (TI*) sc->x;                                                  \
        TI dx = (TI) sc->dx;                                                   \
        int32_t *oi = sc->o;                                                   \
        int32_t *oo = sc->next_o;                                              \
        _Pragma("omp parallel for schedule(dynamic) num_threads(sc->nth)")     \
        for (size_t i = 0; i < sc->nchunks; i++) {                             \
            size_t j0 = i * sc->chunklen,                                      \
                   j1 = minz(j0 + sc->chunklen, sc->n);                        \
            size_t *__restrict__ tcounts = sc->histogram + (sc->nradixes * i); \
            for (size_t j = j0; j < j1; j++) {                                 \
                TI t = xi[j] + dx;                                             \
                size_t k = tcounts[t >> shift]++;                              \
                oo[k] = oi? oi[j] : (int32_t) j;                               \
            }                                                                  \
        }                                                                      \
        assert(sc->histogram[sc->nchunks * sc->nradixes - 1] == sc->n);        \
    }

TEMPLATE_REORDER_DATA(u4u2, uint32_t, uint16_t)
TEMPLATE_REORDER_DATA(u8u4, uint64_t, uint32_t)
TEMPLATE_REORDER_DATA(u8u8, uint64_t, uint64_t)
TEMPLATE_REORDER_DATA_SIMPLE(u1, uint8_t)
TEMPLATE_REORDER_DATA_SIMPLE(u2, uint16_t)
#undef TEMPLATE_REORDER_DATA


static void reorder_data_str(SortContext *sc)
{
    uint16_t *xi = (uint16_t*) sc->x;
    uint16_t *xo = (uint16_t*) sc->next_x;
    int32_t *oi = sc->o;
    int32_t *oo = sc->next_o;
    assert(xo != NULL);
    const unsigned char *strdata = sc->strdata - 1;
    const int32_t *stroffs = sc->stroffs;
    const int32_t strstart = (int32_t) sc->strstart;

    int32_t maxlen = 0;
    #pragma omp parallel for schedule(dynamic) num_threads(sc->nth) \
            reduction(max:maxlen)
    for (size_t i = 0; i < sc->nchunks; i++) {
        size_t j0 = i * sc->chunklen,
               j1 = minz(j0 + sc->chunklen, sc->n);
        size_t *__restrict__ tcounts = sc->histogram + (sc->nradixes * i);
        for (size_t j = j0; j < j1; j++) {
            size_t k = tcounts[xi[j]]++;
            int32_t w = oi? oi[j] : (int32_t) j;
            int32_t offend = stroffs[w];
            int32_t offstart = abs(stroffs[w-1]) + strstart;
            int32_t len = offend - offstart;
            unsigned char c1 = len > 0? strdata[offstart] + 1 : 0;
            unsigned char c2 = len > 1? strdata[offstart+1] + 1 : 0;
            if (len > maxlen) maxlen = len;
            xo[k] = offend < 0? 0 : (uint16_t)((c1 << 8) + c2 + 1);
            oo[k] = w;
        }
    }
    assert(sc->histogram[sc->nchunks * sc->nradixes - 1] == sc->n);
    sc->next_elemsize = 2;
    sc->strmore = maxlen > 2;
}



static void reorder_data(SortContext *sc)
{
    if (!sc->next_x && sc->next_elemsize) {
        dtmalloc_g(sc->next_x, void, sc->n * (size_t)sc->next_elemsize);
    }
    if (!sc->next_o) {
        dtmalloc_g(sc->next_o, int32_t, sc->n);
    }
    switch (sc->elemsize) {
        case 8:
            if (sc->next_elemsize == 8) reorder_data_u8u8(sc);
            else if (sc->next_elemsize == 4) reorder_data_u8u4(sc);
            else goto fail;
            break;
        case 4:
            reorder_data_u4u2(sc);
            break;
        case 2:
            if (sc->next_elemsize == 2) reorder_data_str(sc);
            else if (sc->next_elemsize == 0) reorder_data_u2(sc);
            else goto fail;
            break;
        case 1:
            reorder_data_u1(sc);
            break;
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
    if (!sc->shift) sc->shift = sc->nsigbits - nradixbits;
    sc->nradixes = 1 << nradixbits;
}



/**
 * Sort array `o` of length `n` by values `x`, using MSB Radix sort algorithm.
 * Array `o` will be modified in-place. If `o` is NULL, then it will be
 * allocated and filled with values of `range(n)`.
 *
 * SortContext inputs:
 *      x:      buffer of size `elemsize * n`
 *      next_x: NULL, or buffer of size `next_elemsize * n`
 *      o:      NULL, or array of `int32_t`s of length `n`
 *      next_o: NULL, or array of `int32_t`s of length `n`
 *      elemsize
 *      next_elemsize: (may be zero)
 *      nsigbits
 *
 * SortContext outputs:
 *      o:  If NULL, this array will be allocated; otherwise its contents will
 *          be modified in-place.
 *      x, next_x, next_o, histogram: These arrays may be allocated, or their
 *          contents may be altered arbitrarily.
 */
static void radix_psort(SortContext *sc)
{
    determine_sorting_parameters(sc);
    build_histogram(sc);
    reorder_data(sc);

    if (!sc->x) return;
    if (sc->next_elemsize) {
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

        // Prepare the "next SortContext" variable
        size_t nradixes = sc->nradixes;
        size_t next_elemsize = (size_t) sc->next_elemsize;
        int8_t next_nsigbits = sc->shift;
        if (!next_nsigbits) next_nsigbits = (int8_t)(sc->next_elemsize * 8);
        SortContext next_sc = {
            .x = NULL,
            .o = NULL,
            .n = 0,
            .strdata = sc->strdata,
            .stroffs = sc->stroffs,
            .strstart = sc->strstart + 2,
            .strmore = sc->strmore,
            .elemsize = sc->next_elemsize,
            .issorted = 0,
            .nsigbits = next_nsigbits,
            .shift = 0,
            .nradixes = 0,
            .nth = 0,
            .nchunks = 0,
            .chunklen = 0,
            .histogram = sc->histogram,  // reuse the `histogram` buffer
            .next_x = sc->x,  // x is no longer needed: reuse
            .next_o = NULL,
            .next_elemsize = (int8_t)(sc->strdata? sc->strmore * 2 :
                                      sc->shift > 32? 4 :
                                      sc->shift > 16? 2 : 0),
        };

        // First, determine the sizes of ranges corresponding to each radix that
        // remain to be sorted. Recall that the previous step left us with the
        // `histogram` array containing cumulative sizes of these ranges, so all
        // we need is to diff that array.
        size_t *rrendoffsets = sc->histogram + (sc->nchunks - 1) * sc->nradixes;
        radix_range *rrmap = NULL;
        dtmalloc_g(rrmap, radix_range, sc->nradixes);
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
        // size_t rrlarge = 2 * sc->n / nradixes;
        size_t rrlarge = INSERT_SORT_THRESHOLD;  // for now
        while (rrmap[rri].size > rrlarge && rri < nradixes) {
            size_t off = rrmap[rri].offset;
            next_sc.x = add_ptr(sc->next_x, off * next_elemsize);
            next_sc.o = sc->next_o + off;
            next_sc.n = rrmap[rri].size;
            radix_psort(&next_sc);
            rri++;
        }

        // Finally iterate over all remaining radix ranges, in-parallel, and
        // sort each of them independently using a simpler insertion sort
        // method.
        size_t size0 = rri < nradixes? rrmap[rri].size : 0;
        int32_t *tmp = NULL;
        int own_tmp = 0;
        if (size0) {
            size_t size_all = size0 * sc->nth * sizeof(int32_t);
            if ((size_t)sc->elemsize * sc->n <= size_all) {
                tmp = (int32_t*)sc->x;
            } else {
                own_tmp = 1;
                dtmalloc_g(tmp, int32_t, size0 * sc->nth);
            }
        }
        #pragma omp parallel for schedule(dynamic) num_threads(sc->nth)
        for (size_t i = rri; i < nradixes; i++)
        {
            int me = omp_get_thread_num();
            size_t off = rrmap[i].offset;
            int32_t n = (int32_t) rrmap[i].size;
            if (n <= 1) continue;
            void *x = add_ptr(sc->next_x, off * next_elemsize);
            int32_t *o = sc->next_o + off;
            int32_t *oo = tmp + me * (int32_t)size0;

            if (sc->strdata) {
                int32_t ss = (int32_t) sc->strstart;
                insert_sort_s4_o(sc->strdata, sc->stroffs, ss, o, oo, n);
            } else {
                switch (next_elemsize) {
                    case 1: insert_sort_u1(x, o, oo, n); break;
                    case 2: insert_sort_u2(x, o, oo, n); break;
                    case 4: insert_sort_u4(x, o, oo, n); break;
                    case 8: insert_sort_u8(x, o, oo, n); break;
                }
            }
        }
        dtfree(rrmap);
        if (own_tmp) dtfree(tmp);
    }

    // Done. Save to array `o` the ordering of the input vector `x`.
    if (sc->o) {
        memcpy(sc->o, sc->next_o, sc->n * sizeof(int32_t));
    } else {
        sc->o = sc->next_o;
        sc->next_o = NULL;
    }
    return;
    fail:
    sc->x = NULL;
    sc->o = NULL;
}



//==============================================================================
// Insertion sort functions
//
// All functions here provide the same functionality and have a similar
// signature. Each of these function sorts array `o` according to the values of
// array `x`. Both arrays must have the same length `n`. The caller may also
// provide a temporary buffer `oo` of size at least `4*n` bytes. The contents
// of this array will be overwritten. Returns the pointer `o`.
//
// For example, if `x` is {5, 2, -1, 7, 2}, then this function will leave `x`
// unmodified but reorder the elements of `o` into {o[2], o[1], o[4], o[0],
// o[3]}.
//
// Pointer `o` can be NULL, in which case it will be assumed that `o[i] == i`.
// Similarly, `oo` can be NULL too, in which case it will be allocated inside
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
        const void *__restrict__ x,                                            \
        int32_t *__restrict__ o,                                               \
        int32_t *__restrict__ oo,                                              \
        int32_t n                                                              \
    ) {                                                                        \
        const T *__restrict__ xi = (const T *) x;                              \
        int own_oo = 0;                                                        \
        if (oo == NULL) {                                                      \
            dtmalloc(oo, int32_t, n);                                          \
            own_oo = 1;                                                        \
        }                                                                      \
        oo[0] = 0;                                                             \
        for (int i = 1; i < n; i++) {                                          \
            T xival = xi[i];                                                   \
            int j = i;                                                         \
            while (j && xival < xi[oo[j - 1]]) {                               \
                oo[j] = oo[j - 1];                                             \
                j--;                                                           \
            }                                                                  \
            oo[j] = i;                                                         \
        }                                                                      \
        if (!o) return oo;                                                     \
        for (int i = 0; i < n; i++) {                                          \
            oo[i] = o[oo[i]];                                                  \
        }                                                                      \
        memcpy(o, oo, (size_t)n * sizeof(int32_t));                            \
        if (own_oo) dtfree(oo);                                                \
        return o;                                                              \
    }

DECLARE_INSERT_SORT_FN(i1, int8_t)
DECLARE_INSERT_SORT_FN(i2, int16_t)
DECLARE_INSERT_SORT_FN(i4, int32_t)
DECLARE_INSERT_SORT_FN(i8, int64_t)
DECLARE_INSERT_SORT_FN(u1, uint8_t)
DECLARE_INSERT_SORT_FN(u2, uint16_t)
DECLARE_INSERT_SORT_FN(u4, uint32_t)
DECLARE_INSERT_SORT_FN(u8, uint64_t)
#undef DECLARE_INSERT_SORT_FN


static int32_t* insert_sort_s4_o(
    const unsigned char *__restrict__ strdata,
    const int32_t *__restrict__ stroffs,
    int32_t strstart,
    int32_t *__restrict__ o,
    int32_t *__restrict__ tmp,
    int32_t n
) {
    int32_t j;
    int own_tmp = 0;
    if (tmp == NULL) {
        dtmalloc(tmp, int32_t, n);
        own_tmp = 1;
    }
    tmp[0] = 0;
    const unsigned char *strdata1 = strdata - 1;
    for (int32_t i = 1; i < n; i++) {
        int32_t off0i = abs(stroffs[o[i]-1]) + strstart;
        int32_t off1i = stroffs[o[i]];
        for (j = i; j > 0; j--) {
            int32_t k = tmp[j-1];
            int32_t off0k = abs(stroffs[o[k]-1]) + strstart;
            int32_t off1k = stroffs[o[k]];
            int cmp = _compare_offstrings(strdata1, off0i, off1i, off0k, off1k);
            if (cmp != 1) break;
            tmp[j] = tmp[j-1];
        }
        tmp[j] = i;
    }
    for (int i = 0; i < n; i++) {
        tmp[i] = o[tmp[i]];
    }
    memcpy(o, tmp, (size_t)n * sizeof(int32_t));
    if (own_tmp) dtfree(tmp);
    return o;
}

static int32_t* insert_sort_s4_noo(
    const unsigned char *__restrict__ strdata,
    const int32_t *__restrict__ stroffs,
    int32_t strstart,
    int32_t *__restrict__ tmp,
    int32_t n
) {
    int32_t j;
    if (tmp == NULL) {
        dtmalloc(tmp, int32_t, n);
    }
    tmp[0] = 0;
    const unsigned char *strdata1 = strdata - 1;
    for (int32_t i = 1; i < n; i++) {
        int32_t off0i = abs(stroffs[i-1]) + strstart;
        int32_t off1i = stroffs[i];
        for (j = i; j > 0; j--) {
            int32_t k = tmp[j-1];
            int32_t off0k = abs(stroffs[k-1]) + strstart;
            int32_t off1k = stroffs[k];
            int cmp = _compare_offstrings(strdata1, off0i, off1i, off0k, off1k);
            if (cmp != 1) break;
            tmp[j] = tmp[j-1];
        }
        tmp[j] = i;
    }
    return tmp;
}


static void insert_sort(SortContext *sc)
{
    int32_t n = (int32_t) sc->n;
    if (sc->strdata && sc->strmore) {
        int32_t ss = (int32_t)sc->strstart - 2;
        if (sc->o)
            insert_sort_s4_o(sc->strdata, sc->stroffs,
                             ss, sc->o, (int32_t*)sc->next_x, n);
        else
            sc->o = insert_sort_s4_noo(sc->strdata, sc->stroffs,
                                       ss, (int32_t*)sc->next_x, n);
        return;
    }
    switch (sc->elemsize) {
        case 1: sc->o = insert_sort_u1(sc->x, sc->o, (int32_t*)sc->next_x, n); break;
        case 2: sc->o = insert_sort_u2(sc->x, sc->o, (int32_t*)sc->next_x, n); break;
        case 4: sc->o = insert_sort_u4(sc->x, sc->o, (int32_t*)sc->next_x, n); break;
        case 8: sc->o = insert_sort_u8(sc->x, sc->o, (int32_t*)sc->next_x, n); break;
    }
}



//==============================================================================
// Initializing static arrays
//==============================================================================

void init_sort_functions(void)
{
    for (int i = 0; i < DT_STYPES_COUNT; i++) {
        prepare_inp_fns[i] = NULL;
        insert_sort_fns[i] = NULL;
    }
    prepare_inp_fns[ST_BOOLEAN_I1] = (prepare_inp_fn) &prepare_input_b1;
    prepare_inp_fns[ST_INTEGER_I1] = (prepare_inp_fn) &prepare_input_i1;
    prepare_inp_fns[ST_INTEGER_I2] = (prepare_inp_fn) &prepare_input_i2;
    prepare_inp_fns[ST_INTEGER_I4] = (prepare_inp_fn) &prepare_input_i4;
    prepare_inp_fns[ST_INTEGER_I8] = (prepare_inp_fn) &prepare_input_i8;
    prepare_inp_fns[ST_REAL_F4]    = (prepare_inp_fn) &prepare_input_f4;
    prepare_inp_fns[ST_REAL_F8]    = (prepare_inp_fn) &prepare_input_f8;
    prepare_inp_fns[ST_STRING_I4_VCHAR] = (prepare_inp_fn) &prepare_input_s4;

    insert_sort_fns[ST_BOOLEAN_I1] = (insert_sort_fn) &insert_sort_i1;
    insert_sort_fns[ST_INTEGER_I1] = (insert_sort_fn) &insert_sort_i1;
    insert_sort_fns[ST_INTEGER_I2] = (insert_sort_fn) &insert_sort_i2;
    insert_sort_fns[ST_INTEGER_I4] = (insert_sort_fn) &insert_sort_i4;
    insert_sort_fns[ST_INTEGER_I8] = (insert_sort_fn) &insert_sort_i8;
    insert_sort_fns[ST_REAL_F4]    = (insert_sort_fn) &insert_sort_u4;
    insert_sort_fns[ST_REAL_F8]    = (insert_sort_fn) &insert_sort_u8;
}
