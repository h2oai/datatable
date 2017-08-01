//------------------------------------------------------------------------------
// Sorting/ordering functions.
//
// adapted from
//     https://github.com/Rdatatable/data.table/src/fsort.c and
//     https://github.com/Rdatatable/data.table/src/forder.c
//------------------------------------------------------------------------------
#include <stdint.h>
#include <stdio.h>   // printf
#include <string.h>  // memcpy
#include <omp.h>
#include "myassert.h"
#include "sort.h"
#include "types.h"
#include "utils.h"

// Forward declarations
static void* insert_sort_i4(const int32_t *x, int32_t *y, size_t n, int32_t *restrict temp);
static void* countp_sort_i4(int32_t *restrict x, int32_t *restrict y, size_t n, int32_t *restrict temp, size_t range);
static void* count_sort_i4(int32_t *restrict x, int32_t *restrict y, size_t n, int32_t *restrict temp, size_t range);
static void* radix_sort_i4(int32_t *x, int32_t n, int32_t **o);

static inline size_t maxz(size_t a, size_t b) { return a < b? b : a; }
static inline size_t minz(size_t a, size_t b) { return a < b? a : b; }
static inline void* minp(void *a, void *b) { return a < b? a : b; }
static inline int mini4(int a, int b) { return a < b? a : b; }

#define INSERT_SORT_THRESHOLD 64

typedef struct radix_range { size_t size, offset; } radix_range;
static int _rrcmp(const void *a, const void *b) {
    const size_t x = *(const size_t*)a;
    const size_t y = *(const size_t*)b;
    return (x < y) - (y < x);
}


void* sort_i4(int32_t *x, int32_t n, int32_t **o)
{
    void *ret = NULL;
    if (n <= INSERT_SORT_THRESHOLD) {
        int32_t *oo, *tmp;
        dtmalloc(oo, int32_t, n);
        dtmalloc(tmp, int32_t, n);
        for (int32_t i = 0; i < n; i++) {
            oo[i] = i;
        }
        ret = insert_sort_i4(x, oo, (size_t)n, tmp);
        *o = oo;
        dtfree(tmp);
    } else {
        ret = radix_sort_i4(x, n, o);
    }
    return ret;
}


/**
 * Sort array `x` of length `n` using radix sort algorithm, and return the
 * resulting ordering in variable `o`.
 */
static void* radix_sort_i4(int32_t *x, int32_t n, int32_t **o)
{
    int32_t *xend = x + n;
    int32_t *mins = NULL, *maxs = NULL;
    size_t *counts = NULL;
    int32_t *xx = NULL;
    int32_t *oo = NULL;
    int32_t *tmp = NULL;
    radix_range *rrmap = NULL;

    // Allocate the temporary structures up-front
    dtmalloc(xx, int32_t, n);
    dtmalloc(oo, int32_t, n);


    // Determine how the input should be split into chunks: at least as many
    // chunks as the number of threads, unless the input array is too small
    // and chunks become too small in size. We want to have more than 1 chunk
    // per thread so as to reduce delays caused by uneven execution time among
    // threads, on the other hand too many chunks should be avoided because that
    // would increase the time needed to combine the results from different
    // threads.
    int nth = omp_get_num_threads();
    size_t nz = (size_t) n;
    size_t nchunks = (size_t) nth * 2;
    size_t chunklen = maxz(1024, (nz + nchunks - 1) / nchunks);
    nchunks = (nz - 1)/chunklen + 1;


    // Compute the min/max of the data column, in parallel.
    // TODO: replace this with the information from RollupStats once those are
    //       implemented.
    dtmalloc(mins, int32_t, nchunks);
    dtmalloc(maxs, int32_t, nchunks);

    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        int32_t *myx = x + (chunklen * i);
        int32_t *myxend = minp(myx + chunklen, xend);
        int32_t mymin = *myx;
        int32_t mymax = *myx;
        for (myx++; myx < myxend; myx++) {
            int32_t t = *myx;
            if (t < mymin)
                mymin = t;
            else if (t > mymax)
                mymax = t;
        }
        mins[i] = mymin;
        maxs[i] = mymax;
    }

    int32_t min = mins[0];
    int32_t max = maxs[0];
    for (size_t i = 1; i < nchunks; i++) {
        if (mins[i] < min) min = mins[i];
        if (maxs[i] > max) max = maxs[i];
    }
    dtfree(mins);
    dtfree(maxs);


    // Compute the optimal radix size.
    // nsigbits: number of significant bits in `x`s, after subtracting the min
    // nradixbits: how many bits to use for the radix
    // shift: the right-shift needed to leave the radix only
    // radixcount: how many different radixes there may be (+1 for NAs). This
    //     number does not exceed 65537.
    int nsigbits = 32 - (int) nlz((uint32_t)(max - min));
    int nradixbits = mini4(nsigbits, 16);
    int shift = nsigbits - nradixbits;
    size_t radixcount = (1 << nradixbits) + 1;


    // Calculate initial histograms of values in `x`. Specifically, we're
    // creating the `counts` table which has `nchunks` rows and `radixcount`
    // columns. Cell `[i,j]` in this table will contain the count of values
    // `x` within the chunk `i` such that the topmost `nradixbits` of `x - min`
    // are equal to `j`.
    // More precisely, we put all `x`s equal to NA into cell 0, while other
    // values into the cell `1 + ((x - min)>>shift)`. The expression `x - min`
    // cannot be computed as-is however: it is possible that it overflows the
    // range of int32_t, in which case the behavior is undefined. However
    // casting both of them to uint32_t first avoids this problem and produces
    // the correct result.
    // Allocate `(nchunks + 2)` because we will need 2 more rows to hold the
    // `rrmap` array (see [*]).
    dtcalloc(counts, size_t, (nchunks + 2) * radixcount);
    uint32_t umin = (uint32_t) min;
    uint32_t una = (uint32_t) NA_I4;
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        uint32_t *restrict myx = (uint32_t*)(x) + (chunklen * i);
        uint32_t *myxend = (uint32_t*) minp(myx + chunklen, xend);
        size_t *restrict mycounts = counts + (radixcount * i);
        for (; myx < myxend; myx++) {
            uint32_t radix = (*myx == una) ? 0 : ((*myx - umin) >> shift) + 1;
            mycounts[radix]++;
        }
    }


    // Modify the `counts` table so that it contains the columnar cumulative
    // sums of the previous counts. After this step, cell `[i,j]` of the table
    // will contain the starting index (within the final answer) of where the
    // values `x` with radix `j` within the chunk `i` should be placed.
    // Since the `counts` table is relatively small, no need to go parallel
    // here.
    size_t counts_size = nchunks * radixcount;
    size_t cumsum = 0;
    for (size_t j = 0; j < radixcount; j++) {
        for (size_t offset = j; offset < counts_size; offset += radixcount) {
            size_t t = counts[offset];
            counts[offset] = cumsum;
            cumsum += t;
        }
    }


    // Perform the main radix shuffle, filling in arrays `oo` and `xx`. The
    // array `oo` will contain the original row numbers of the values in `x`
    // such that `x[oo]` is sorted with respect to the most significant bits.
    // The `xx` array will contain the sorted elements of `x`, with MSB bits
    // already removed -- we need it mostly for page-efficiency at later stages
    // of the algorithm.
    // During execution of this step we also modify the `counts` array, again,
    // so that by the end of the step each cell in `counts` will contain the
    // offset past the *last* item within that cell. In particular, the last row
    // of the `counts` table will contain end-offsets of output regions
    // corresponding to each radix value.
    // This is the last step that accesses the input array in chunks.
    uint32_t mask = (1 << shift) - 1;
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        uint32_t *ux = (uint32_t*) x;
        uint32_t *restrict myx = ux + (chunklen * i);
        uint32_t *myxend = (uint32_t*) minp(myx + chunklen, xend);
        size_t *restrict mycounts = counts + (radixcount * i);
        for (; myx < myxend; myx++) {
            uint32_t radix = (*myx == una) ? 0 : ((*myx - umin) >> shift) + 1;
            size_t k = mycounts[radix]++;
            oo[k] = (int32_t)(myx - ux);
            xx[k] = (int32_t)((*myx - umin) & mask);
        }
    }
    assert(counts[counts_size - 1] == (size_t)n);


    if (shift == 0) {
        // Nothing left to do: the array got sorted in the first pass
    } else
    {
        // Prepare temporary buffer
        size_t tmpsize = maxz((size_t)nth << shift, INSERT_SORT_THRESHOLD);
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
        // output array. The `rrmap` array holds `radixcount - 1` elements ("-1"
        // because we skip the NAs bin, where all elements are already sorted),
        // which fits into the dimensions of `counts` [*].
        size_t *rrendoffsets = counts + (nchunks - 1) * radixcount;
        rrmap = (radix_range*)(counts + counts_size);
        for (int64_t i = radixcount - 2; i >= 0; i--) {
            size_t start = rrendoffsets[i];
            size_t end = rrendoffsets[i + 1];
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
        qsort(rrmap, radixcount - 1, sizeof(radix_range), _rrcmp);
        assert(rrmap[0].size >= rrmap[radixcount - 2].size);

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
        // situation).
        size_t rri = 0;
        size_t rrlarge = 2 * (size_t)n / radixcount;
        while (rrmap[rri].size > rrlarge && rri < radixcount - 1) {
            size_t off = rrmap[rri].offset;
            countp_sort_i4(xx + off, oo + off, rrmap[rri].size,
                            tmp, 1 << shift);
            rri++;
        }

        // Finally iterate over all remaining radix ranges, in-parallel, and
        // sort each of them independently using one of the simpler algorithms:
        // counting sort or insertion sort.
        #pragma omp parallel for num_threads(nth)
        for (size_t i = rri; i < radixcount - 1; i++)
        {
            size_t off = rrmap[i].offset;
            size_t size = rrmap[i].size;
            if (size <= INSERT_SORT_THRESHOLD) {
                insert_sort_i4(xx + off, oo + off, size, tmp);
            } else {
                count_sort_i4(xx + off, oo + off, size, tmp, 1 << shift);
            }
        }
    }

    // Done. Array `oo` contains the ordering of the input vector `x`.
    dtfree(xx);
    dtfree(tmp);
    *o = oo;
    return (void*)o;
}




/**
 * Sort array `y` according to the values of array `x`. Both arrays have same
 * size `n`. For example, if `x` is {5, 2, -1, 7, 2}, then this function will
 * leave `x` unmodified, and reorder the elements of `y` into {y[2], y[1], y[4],
 * y[0], y[3]}.
 * The caller should also provide temporary buffer `tmp` of the size at least
 * `n`. This temporary buffer must be distinct from `x` or `y`, and its contents
 * will be overwritten.
 *
 * This procedure uses Insert Sort algorithm, which has O(n²) complexity.
 * Therefore, this procedure should only be used for small arrays.
 *
 * See also:
 *   - https://en.wikipedia.org/wiki/Insertion_sort
 *   - datatable/microbench/insertsort
 */
static void* insert_sort_i4(
    const int32_t *restrict x,
    int32_t *restrict y,
    size_t n,
    int32_t *restrict tmp
) {
    int ni = (int) n;
    tmp[0] = 0;
    for (int i = 1; i < ni; i++) {
        int32_t xi = x[i];
        int j = i;
        while (j && xi < x[tmp[j - 1]]) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = i;
    }
    for (int i = 0; i < ni; i++) {
        tmp[i] = y[tmp[i]];
    }
    memcpy(y, tmp, n * sizeof(int32_t));
    return NULL;
}


/**
 * Sort array `y` according to the values of array `x`. Both arrays have same
 * size `n`. Array `x` will be overwritten in the process.
 * It is assumed that array `x` contains values in the range `0 .. range - 1`,
 * and the temporary buffer `tmp` has the size sufficient for `nthreads * range`
 * integers.
 *
 * This procedure uses parallel Counting Sort algorithm.
 */
static void* countp_sort_i4(
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

    // Reorder the data
    #pragma omp parallel for schedule(dynamic) num_threads(nth)
    for (size_t i = 0; i < nchunks; i++)
    {
        int32_t *restrict myx = x + i * chunklen;
        int32_t *const myxend = x + minz((i + 1) * chunklen, n);
        int32_t *restrict mycounts = tmp + (i * range);
        for (; myx < myxend; myx++) {
            int32_t k = mycounts[*myx]++;
            *myx = y[k];
        }
    }

    // Produce the output
    memcpy(y, x, n * sizeof(int32_t));
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
