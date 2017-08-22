//==============================================================================
// Micro benchmark for radix sort function
//==============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "sort.h"


// tmp1 should be at least `1 << K` ints long,
// tmp2 should have at least `n` ints.
void radixsort0(int *x, int *o, int n, int K)
{
    int *histogram = tmp1;
    int *oo = tmp2;

    int nradixes = 1 << K;
    memset(histogram, 0, nradixes * sizeof(int));

    // Generate the histogram
    for (int i = 0; i < n; i++) {
        histogram[x[i]]++;
        // if (x[i] < 0 || x[i] >= nradixes) exit(1);
    }
    int cumsum = 0;
    for (int i = 0; i < nradixes; i++) {
        int h = histogram[i];
        histogram[i] = cumsum;
        cumsum += h;
    }
    // printf("histogram = ["); for (int i = 0; i < nradixes; i++)printf("%d, ", histogram[i]); printf("\b\b]\n");

    // Sort the variables using the histogram
    for (int i = 0; i < n; i++) {
        int k = histogram[x[i]]++;
        // if (k < 0 || k >= n) { printf("k=%d at i=%d\n", k, i); exit(2); }
        oo[k] = o[i];
    }
    memcpy(o, oo, n * sizeof(int));
}
