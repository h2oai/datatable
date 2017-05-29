#include <stdint.h>

// Forward declarations
static void insert_sort_i32(int32_t *x, int32_t *o, int32_t n);


void sort_i32(int32_t *x, int32_t n, int32_t **o)
{

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
static void insert_sort_i32(
    const int32_t *x, int32_t *y, int32_t n, int32_t* restrict tmp
) {
    tmp[0] = 0;
    for (int32_t i = 1; i < n; i++) {
        int32_t xi = x[i];
        int32_t j = i;
        while (j && xi < x[tmp[j - 1]]) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = i;
    }
    for (int32_t i = 0; i < n; i++) {
        tmp[i] = y[tmp[i]];
    }
    memcpy(y, tmp, n * sizeof(int32_t));
}

