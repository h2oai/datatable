//==============================================================================
//
// Micro benchmark for merge-sort function.
// Example:
//
//     make run size=64 batches=100 iters=1000
//
// Will run the benchmark 100 times, each time doing 1000 iterations, working
// on an array of 64 integers. This will be done for each available "kernel":
//     mergesort0: top-down mergesort
//
//==============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include "utils.h"


static void radixsort0(const int *x, int *o, int n, int nsigbits,
                       int *histogram, int *oo)
{
    int nradixes = 1 << nsigbits;
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


//==============================================================================
// Program main
//==============================================================================
int main(int argc, char **argv)
{
    // Parse command-line arguments
    int n = getCmdArgInt(argc, argv, "size", 64);
    int iters = getCmdArgInt(argc, argv, "iters", 100);
    int nbatches = getCmdArgInt(argc, argv, "batches", 20);
    printf("Array size = %d ints\n", n);
    printf("Number of batches = %d\n", nbatches);
    printf("Number of iterations per batch = %d\n", iters);

    size_t alloc_size = (size_t)n * sizeof(int);
    int *x = malloc(alloc_size);
    int *o = malloc(alloc_size);
    int *wx = malloc(alloc_size);
    int *wo = malloc(alloc_size);
    int *tmp = malloc(alloc_size);
    int *histogram = malloc(65536 * sizeof(int));

    for (int t = 0; t < 8; t++)
    {
        int nsigbits = 2 + 2 * t;
        int radix = 1 << nsigbits;

        double total_time = 0;
        for (int b = 0; b < nbatches; b++)
        {
            // Prepare data array
            srand(time(NULL));
            for (int i = 0; i < n; i++) {
                x[i] = rand() % radix;
                o[i] = i;
                assert(0 <= x[i] && x[i] < radix);
            }
            // printf("radix = %d\n", radix);
            // printf("x = ["); for (int i = 0; i < n; i++)printf("%d, ", x[i]); printf("\b\b]\n");

            // Check correctness
            memcpy(wo, o, alloc_size);
            radixsort0(x, wo, n, nsigbits, histogram, tmp);
            for (int i = 0; i < n; i++) {
                if (i > 0 &&
                        !(x[wo[i]] > x[wo[i-1]] || (x[wo[i]] == x[wo[i-1]] && wo[i] > wo[i-1]))
                ) {
                    printf("Results are incorrect! (at i = %d)\n", i);
                    printf("  Input x: ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("]\n");
                    printf("  Sorted x: ["); for(int i = 0; i < n; i++) printf("%d, ", x[wo[i]]); printf("]\n");
                    return 1;
                }
            }

            // Kernel 0
            start_timer();
            for (int i = 0; i < iters; i++) {
                memcpy(wo, o, alloc_size);
                radixsort0(x, wo, n, nsigbits, histogram, tmp);
            }
            total_time += get_timer_iter(iters);
        }

        printf("@ radixsort-%d:  mean = %.2f ns\n",
               nsigbits, total_time / nbatches);
    }

    return 0;
}
