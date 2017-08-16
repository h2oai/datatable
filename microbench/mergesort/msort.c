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
#include <assert.h>
#include "utils.h"

static int *base = NULL;

static void mergesort0(int *x, int *o, int n, int *restrict t, int *restrict u, int indent)
{
    // char IND[100]; for(int i=0;i<indent*2;i++) IND[i] = ' '; IND[indent] = '\0';
    // printf("%smergesort0(@%d, n=%d)\n", IND, x-base, n);
    // printf("%s  x_in = [", IND); for(int i=0; i<n;i++) printf("%d, ", x[i]); printf("\b\b]\n");
    if (n <= 2) {
        if (n == 2 && x[0] > x[1]) {
            int t = o[0];
            o[0] = o[1];
            o[1] = t;
            t = x[0];
            x[0] = x[1];
            x[1] = t;
        }
        // printf("%s  x_out = [", IND); for(int i=0; i<n;i++) printf("%d, ", x[i]); printf("\b\b]\n");
        return;
    }
    // Sort each part recursively
    int n1 = n / 2;
    int n2 = n - n1;
    mergesort0(x, o, n1, t, u, indent+2);
    mergesort0(x + n1, o + n1, n2, t + n1, u + n1, indent+2);

    // Merge the parts
    memcpy(t, x, n1 * sizeof(int));
    memcpy(u, o, n1 * sizeof(int));
    int i = 0, j = 0, k = 0;
    int *restrict x1 = t;
    int *restrict x2 = x + n1;
    int *restrict o1 = u;
    int *restrict o2 = o + n1;
    while (1) {
        if (x1[i] <= x2[j]) {
            x[k] = x1[i];
            o[k] = o1[i];
            k++; i++;
            if (i == n1) {
                assert(j + n1 == k);
                break;
            }
        } else {
            x[k] = x2[j];
            o[k] = o2[j];
            k++; j++;
            if (j == n2) {
                memcpy(x + k, x1 + i, (n1 - i) * sizeof(int));
                memcpy(o + k, o1 + i, (n1 - i) * sizeof(int));
                break;
            }
        }
    }
    // printf("%s  x_out = [", IND); for(int i=0; i<n;i++) printf("%d, ", x[i]); printf("\b\b]\n");
}


//==============================================================================
// Program main
//==============================================================================
int main(int argc, char **argv)
{
    // Parse command-line arguments
    int size = getCmdArgInt(argc, argv, "size", 64);
    int iters = getCmdArgInt(argc, argv, "iters", 1000);
    int nbatches = getCmdArgInt(argc, argv, "batches", 100);
    printf("Array size = %d ints\n", size);
    printf("Number of batches = %d\n", nbatches);
    printf("Number of iterations per batch = %d\n", iters);

    double total_time0 = 0, min_time0 = 0, max_time0 = 0;
    double total_time2 = 0, min_time2 = 0, max_time2 = 0;
    double total_time3 = 0, min_time3 = 0, max_time3 = 0;

    for (int b = 0; b < nbatches; b++)
    {
        // Prepare data array
        // srand(123456789);
        size_t alloc_size = (size_t)size * sizeof(int);
        int *x = malloc(alloc_size);
        int *o = malloc(alloc_size);
        for (int i = 0; i < size; i++) {
            x[i] = rand() % 10000;
            o[i] = i;
        }
        int *wx = malloc(alloc_size);
        int *wo = malloc(alloc_size);
        int *tmp = malloc(alloc_size * 2);

        // Check correctness
        memcpy(wx, x, alloc_size);
        memcpy(wo, o, alloc_size);
        base = wx;
        mergesort0(wx, wo, size, tmp, tmp + size, 0);
        for (int i = 0; i < size; i++) {
            if (i > 0 &&
                    !(x[wo[i]] > x[wo[i-1]] || (x[wo[i]] == x[wo[i-1]] && wo[i] > wo[i-1]))
            ) {
                printf("Results are incorrect! (at i = %d)\n", i);
                printf("  Input x: ["); for(int i = 0; i < size; i++) printf("%d, ", x[i]); printf("]\n");
                printf("  Sorted x: ["); for(int i = 0; i < size; i++) printf("%d, ", x[wo[i]]); printf("]\n");
                return 1;
            }
        }

        // Kernel 0
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wo, o, alloc_size);
            mergesort0(wx, wo, size, tmp, tmp + size, 0);
        }
        double t0 = get_timer_iter(iters);
        total_time0 += t0;
        if (b == 0 || t0 < min_time0) min_time0 = t0;
        if (b == 0 || t0 > max_time0) max_time0 = t0;
    }

    printf("@ mergesort0:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time0 / nbatches, min_time0, max_time0);

    return 0;
}
