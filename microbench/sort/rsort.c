//==============================================================================
// Micro benchmark for radix sort function
//==============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "sort.h"


void bestsort(int *x, int *o, int N, int K)
{
    static int INSERT_THRESHOLDS[] = {0, 8, 8, 8, 8, 12, 16, 16, 20, 20, 20, 20, 20,
                                      20, 20, 20, 20};
    if (N <= INSERT_THRESHOLDS[K]) {
        iinsert0_i4(x, o, N, K);
    } else {
        radixsort0(x, o, N, K);
    }
}

void bestsort_i1(uint8_t *x, int *o, int N, int K)
{
    static int INSERT_THRESHOLDS[] = {0, 8, 8, 8, 8, 12, 16, 16, 20, 20, 20, 20, 20,
                                      20, 20, 20, 20};
    if (N <= INSERT_THRESHOLDS[K]) {
        iinsert0_i1(x, o, N, K);
    } else {
        radixsort0_i1(x, o, N, K);
    }
}



// Regular Radix Sort, using all K significant digits.
// tmp2 should have at least `n` ints.
// tmp3 should be at least `1 << K` ints long,
void radixsort0(int *x, int *o, int n, int K)
{
    int *oo = tmp2;
    int *histogram = tmp3;

    int nradixes = 1 << K;
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    for (int i = 0; i < n; i++) {
        histogram[x[i]]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }

    // Sort the variables using the histogram
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i]]++;
        oo[k] = o[i];
    }
    memcpy(o, oo, n * sizeof(int));
}

void radixsort0_i1(uint8_t *x, int *o, int n, int K)
{
    int *oo = tmp2;
    int *histogram = tmp3;
    int nradixes = 1 << K;
    memset(histogram, 0, nradixes * sizeof(int));
    for (int i = 0; i < n; i++) {
        histogram[x[i]]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i]]++;
        oo[k] = o[i];
    }
    memcpy(o, oo, n * sizeof(int));
}


// Radix Sort that first partially sorts by `tmp0` MSB bits, and then sorts
// the remaining numbers using "best" sort.
void radixsort1(int *x, int *o, int n, int K)
{
    // printf("radixsort1(x=%p, o=%p, n=%d, K=%d)\n", x, o, n, K);
    int nradixbits = tmp0;
    int *xx = tmp1;
    int *oo = tmp2;
    int *histogram = tmp3;

    int nradixes = 1 << nradixbits;
    int shift = K - nradixbits;
    int mask = nradixes - 1;
    // printf("  nradixes=%d, shift=%d, mask=%d\n", nradixes, shift, mask);
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    // printf("  generate histogram...\n");
    for (int i = 0; i < n; i++) {
        histogram[x[i] >> shift]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }

    // Sort the variables using the histogram
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i] >> shift]++;
        xx[k] = x[i] & mask;
        oo[k] = o[i];
    }

    // Continue sorting the remainder
    tmp1 = x;
    tmp2 = o;
    for (int i = 0; i < nradixes; i++) {
        int start = i? histogram[i - 1] : 0;
        int end = histogram[i];
        int nextn = end - start;
        if (nextn <= 1) continue;
        int *nextx = xx + start;
        int *nexto = oo + start;
        bestsort(nextx, nexto, nextn, shift);
    }
    tmp1 = xx;
    tmp2 = oo;

    memcpy(o, oo, n * sizeof(int));
}



// Radix Sort that first partially sorts by `tmp0` MSB bits, and then sorts
// the remaining numbers using again a radix sort.
// Note that the driver script allocates `1 << K` ints for buffer `tmp3`.
// We use here only `1 << tmp0` for the histogram, and then `1 << (K - tmp0)`
// for the recursive calls.
void radixsort2(int *x, int *o, int n, int K)
{
    // printf("radixsort2(x=%p, o=%p, n=%d, K=%d [tmp0=%d, tmp1=%p, tmp2=%p, tmp3=%p])\n",
    //        x, o, n, K, tmp0, tmp1, tmp2, tmp3);
    int nradixbits = tmp0;
    int *xx = tmp1;
    int *oo = tmp2;
    int *histogram = tmp3;

    int nradixes = 1 << nradixbits;
    int shift = K - nradixbits;
    int mask = (1 << shift) - 1;
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    for (int i = 0; i < n; i++) {
        // assert((x[i] >> shift) < nradixes && (x[i]>>shift) >= 0);
        histogram[x[i] >> shift]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }

    // Sort the variables using the histogram
    for (int i = 0; i < n; i++) {
        // assert((x[i] >> shift) < nradixes && (x[i]>>shift) >= 0);
        int k = histogram[x[i] >> shift]++;
        xx[k] = x[i] & mask;
        oo[k] = o[i];
        // assert(xx[k] >= 0 && xx[k] < (1 << shift));
    }

    // Continue sorting the remainder
    tmp2 = o;
    tmp3 = histogram + nradixes;
    for (int i = 0; i < nradixes; i++) {
        int start = i? histogram[i - 1] : 0;
        int end = histogram[i];
        int nextn = end - start;
        if (nextn <= 1) continue;
        int *nextx = xx + start;
        int *nexto = oo + start;
        if (nextn <= 6) {
            iinsert0_i4(nextx, nexto, nextn, shift);
        } else {
            // This will also use (and modify) tmp1 and tmp2
            radixsort0(nextx, nexto, nextn, shift);
        }
    }
    tmp2 = oo;
    tmp3 = histogram;

    memcpy(o, oo, n * sizeof(int));
    // printf("  done.\n");
}

void radixsort2_i1(uint8_t *x, int *o, int n, int K)
{
    int nradixbits = tmp0;
    uint8_t *xx = (uint8_t*)tmp1;
    int *oo = tmp2;
    int *histogram = tmp3;

    int nradixes = 1 << nradixbits;
    int shift = K - nradixbits;
    uint8_t mask = (uint8_t)((1 << shift) - 1);
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    for (int i = 0; i < n; i++) {
        histogram[x[i] >> shift]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }

    // Sort the variables using the histogram
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i] >> shift]++;
        xx[k] = x[i] & mask;
        oo[k] = o[i];
    }

    // Continue sorting the remainder
    tmp2 = o;
    tmp3 = histogram + nradixes;
    for (int i = 0; i < nradixes; i++) {
        int start = i? histogram[i - 1] : 0;
        int end = histogram[i];
        int nextn = end - start;
        if (nextn <= 1) continue;
        uint8_t *nextx = xx + start;
        int *nexto = oo + start;
        bestsort_i1(nextx, nexto, nextn, shift);
    }
    tmp2 = oo;
    tmp3 = histogram;

    memcpy(o, oo, n * sizeof(int));
    // printf("  done.\n");
}


//------------------------------------------------------------------------------
// This is exactly like radixsort2, but stores output x array more compactly:
// either as uint8_t or uint16_t.
//------------------------------------------------------------------------------
static void iinsert0_i2(uint16_t *x, int *o, int n)
{
    for (int i = 1; i < n; i++) {
        uint16_t xtmp = x[i];
        if (xtmp < x[i-1]) {
            int j = i - 1;
            int otmp = o[i];
            while (j >= 0 && xtmp < x[j]) {
                x[j+1] = x[j];
                o[j+1] = o[j];
                j--;
            }
            x[j+1] = xtmp;
            o[j+1] = otmp;
        }
    }
}

static void radixsort0_i2(uint16_t *x, int *o, int n, int K, int *oo, int *histogram)
{
    int nradixes = 1 << K;
    memset(histogram, 0, nradixes * sizeof(int));
    for (int i = 0; i < n; i++) {
        histogram[x[i]]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i]]++;
        oo[k] = o[i];
    }
    memcpy(o, oo, n * sizeof(int));
}

void radixsort3(int *x, int *o, int n, int K)
{
    int nradixbits = tmp0;
    int *oo = tmp2;
    int *histogram = tmp3;

    int nradixes = 1 << nradixbits;
    int shift = K - nradixbits;
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    for (int i = 0; i < n; i++) {
        histogram[x[i] >> shift]++;
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }

    tmp2 = o;
    tmp3 = histogram + nradixes;
    if (shift <= 8) {
        // Sort the variables using the histogram
        uint8_t *xx = (uint8_t*) tmp1;
        uint8_t mask = (uint8_t)((1 << shift) - 1);
        for (int i = 0; i < n; i++) {
            int k = histogram[x[i] >> shift]++;
            xx[k] = (uint8_t)(x[i]) & mask;
            oo[k] = o[i];
        }

        // Continue sorting the remainder
        for (int i = 0; i < nradixes; i++) {
            int start = i? histogram[i - 1] : 0;
            int end = histogram[i];
            int nextn = end - start;
            if (nextn <= 1) continue;
            uint8_t *nextx = xx + start;
            int *nexto = oo + start;
            if (nextn <= 6) {
                iinsert0_i1(nextx, nexto, nextn, shift);
            } else {
                radixsort0_i1(nextx, nexto, nextn, shift);
            }
        }
    } else if (shift <= 16) {
        // Sort the variables using the histogram
        uint16_t *xx = (uint16_t*) tmp1;
        uint16_t mask = (uint16_t)((1 << shift) - 1);
        for (int i = 0; i < n; i++) {
            int k = histogram[x[i] >> shift]++;
            xx[k] = (uint16_t)(x[i]) & mask;
            oo[k] = o[i];
        }

        // Continue sorting the remainder
        for (int i = 0; i < nradixes; i++) {
            int start = i? histogram[i - 1] : 0;
            int end = histogram[i];
            int nextn = end - start;
            if (nextn <= 1) continue;
            uint16_t *nextx = xx + start;
            int *nexto = oo + start;
            if (nextn <= 6) {
                iinsert0_i2(nextx, nexto, nextn);
            } else {
                radixsort0_i2(nextx, nexto, nextn, shift,
                              o, histogram + nradixes);
            }
        }
    } else {
        assert(0);
    }
    tmp2 = oo;
    tmp3 = histogram;

    memcpy(o, oo, n * sizeof(int));
}
