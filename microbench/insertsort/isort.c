//==============================================================================
//
// Micro benchmark for insert-sort function.
// Example:
//
//     make run size=64 batches=100 iters=1000
//
// Will run the benchmark 100 times, each time doing 1000 iterations, working
// on an array of 64 integers. This will be done for each available "kernel":
//     iinsert0: original function taken from forder.c,
//     iinsert2: an attempt to optimize iinsert0
//     iinsert3: two-way insert sort from Knuth Vol.3
//
//==============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include "utils.h"


static void iinsert0(int *x, int *o, int n)
{
    int i, j, xtmp, otmp;
    for (i = 1; i < n; i++) {
        xtmp = x[i];
        if (xtmp < x[i-1]) {
            j = i - 1;
            otmp = o[i];
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


static void iinsert2(const int *x, int *o, int n, int* restrict t)
{
    t[0] = 0;
    for (int i = 1; i < n; i++) {
        int xi = x[i];
        int j = i;
        while (j && xi < x[t[j - 1]]) {
            t[j] = t[j - 1];
            j--;
        }
        t[j] = i;
    }
    for (int i = 0; i < n; i++) {
        t[i] = o[t[i]];
    }
    memcpy(o, t, n * sizeof(int));
}


// Two-way insert sort (see Knuth Vol.3)
static void iinsert3(const int *x, int *o, int n, int *restrict t)
{
    t[n] = 0;
    int r = n, l = n, j, xr, xl;
    xr = xl = x[0];
    for (int i = 1; i < n; i++) {
        int xi = x[i];
        if (xi >= xr) {
            t[++r] = i;
            xr = xi;
        } else if (xi < xl) {
            t[--l] = i;
            xl = xi;
        } else {
            // Compute `j` such that `xi` has to be inserted between elements
            // `j` and `j-1`, i.e. such that `x[t[j-1]] <= xi < x[t[j]]`.
            if (xi < x[t[n]]) {
                j = n - 1;
                while (xi < x[t[j]]) j--;
                j++;
            } else {
                j = n + 1;
                while (xi >= x[t[j]]) j++;
            }
            // assert(x[t[j - 1]] <= xi && xi < x[t[j]]);
            int rshift = r - j + 1;
            int lshift = j - l;
            if (rshift <= lshift) {
                // shift elements [j .. r] upwards by 1
                for (int k = r; k >= j; k--) {
                    t[k + 1] = t[k];
                }
                r++;
                t[j] = i;
            } else {
                // shift elements [l .. j-1] downwards by 1
                for (int k = l; k < j; k++) {
                    t[k - 1] = t[k];
                }
                l--;
                t[j-1] = i;
            }
        }
    }
    // assert(r - l + 1 == n);
    for (int i = l; i <= r; i++) {
        t[i] = o[t[i]];
    }
    memcpy(o, t + l, n * sizeof(int));
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
        int *y = malloc(alloc_size);
        for (int i = 0; i < size; i++) {
            x[i] = rand();
            y[i] = i;
        }
        int *wx = malloc(alloc_size);
        int *wy = malloc(alloc_size);
        int *tmp = malloc(alloc_size * 2);

        // Check correctness
        memcpy(wx, x, alloc_size);
        memcpy(wy, y, alloc_size);
        iinsert0(wx, wy, size);
        int *copy1 = malloc(alloc_size);
        memcpy(copy1, wy, alloc_size);
        memcpy(wx, x, alloc_size);
        memcpy(wy, y, alloc_size);
        iinsert2(wx, wy, size, tmp);
        int *copy2 = malloc(alloc_size);
        memcpy(copy2, wy, alloc_size);
        memcpy(wx, x, alloc_size);
        memcpy(wy, y, alloc_size);
        iinsert3(wx, wy, size, tmp);
        for (int i = 0; i < size; i++) {
            if (copy1[i] != wy[i] || copy2[i] != wy[i] || (i > 0 &&
                    !(x[wy[i]] > x[wy[i-1]] || (x[wy[i]] == x[wy[i-1]] && wy[i] > wy[i-1]))
                    )
            ) {
                printf("Results are different! (at i = %d)\n", i);
                printf("  Input x: ["); for(int i = 0; i < size; i++) printf("%d, ", x[i]); printf("]\n");
                printf("  Sorted x: ["); for(int i = 0; i < size; i++) printf("%d, ", x[wy[i]]); printf("]\n");
                printf("  Out 1: ["); for(int i = 0; i < size; i++) printf("%d, ", copy1[i]); printf("]\n");
                printf("  Out 2: ["); for(int i = 0; i < size; i++) printf("%d, ", copy2[i]); printf("]\n");
                printf("  Out 3: ["); for(int i = 0; i < size; i++) printf("%d, ", wy[i]); printf("]\n");
                return 1;
            }
        }

        // Kernel 0
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wy, y, alloc_size);
            iinsert0(wx, wy, size);
        }
        double t0 = get_timer_iter(iters);
        total_time0 += t0;
        if (b == 0 || t0 < min_time0) min_time0 = t0;
        if (b == 0 || t0 > max_time0) max_time0 = t0;

        // Kernel 2
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wy, y, alloc_size);
            iinsert2(wx, wy, size, tmp);
        }
        double t2 = get_timer_iter(iters);
        total_time2 += t2;
        if (b == 0 || t2 < min_time2) min_time2 = t2;
        if (b == 0 || t2 > max_time2) max_time2 = t2;

        // Kernel 3
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wy, y, alloc_size);
            iinsert3(wx, wy, size, tmp);
        }
        double t3 = get_timer_iter(iters);
        total_time3 += t3;
        if (b == 0 || t3 < min_time3) min_time3 = t3;
        if (b == 0 || t3 > max_time3) max_time3 = t3;
    }

    printf("@ iinsert0:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time0 / nbatches, min_time0, max_time0);
    printf("@ iinsert2:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time2 / nbatches, min_time2, max_time2);
    printf("@ iinsert3:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time3 / nbatches, min_time3, max_time3);
    return 0;
}
