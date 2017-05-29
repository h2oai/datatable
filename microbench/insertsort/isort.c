//==============================================================================
//
// Micro benchmark for insert-sort function.
// Example:
//
//     ./isort size=64 iters=100000 kernel=0
//
// Will run the benchmark 100,000 times on an array of 64 integers using
// "kernel 0". Here "kernel 0" is the original function taken from forder.c,
// and all other kernels are various attempts at improving the original kernel.
//
//==============================================================================
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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

static void iinsert1(int *x, uint8_t *o, uint8_t n)
{
    for (uint8_t i = 1; i < n; i++) {
        int xtmp = x[i];
        if (xtmp < x[i-1]) {
            uint8_t j = i - 1;
            uint8_t otmp = o[i];
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


static void iinsert2(const int *x, int *y, int n, int* restrict tmp)
{
    tmp[0] = 0;
    for (int i = 1; i < n; i++) {
        int xi = x[i];
        int j = i;
        while (j && xi < x[tmp[j - 1]]) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = i;
    }
    for (int i = 0; i < n; i++) {
        tmp[i] = y[tmp[i]];
    }
    memcpy(y, tmp, n * sizeof(int));
}



//==============================================================================
// Program main
//==============================================================================
int main(int argc, char **argv)
{
    // Parse command-line arguments
    int size = getCmdArgInt(argc, argv, "size", 64);
    int iters = getCmdArgInt(argc, argv, "iters", 1000000);
    int kernel = getCmdArgInt(argc, argv, "kernel", 0);
    printf("Array size = %d\n", size);
    printf("Number of iterations = %d\n", iters);
    printf("Kernel = %d\n", kernel);

    // Prepare data array
    srand(123456789);
    size_t alloc_size = (size_t)size * sizeof(int);
    int *x = malloc(alloc_size);
    int *y = malloc(alloc_size);
    for (int i = 0; i < size; i++) {
        x[i] = rand();
        y[i] = i * 1000;
    }
    int *wx = malloc(alloc_size);
    int *wy = malloc(alloc_size);
    int *tmp = malloc(alloc_size);

    // Check correctness
    memcpy(wx, x, alloc_size);
    memcpy(wy, y, alloc_size);
    iinsert0(wx, wy, size);
    int *copy = malloc(alloc_size);
    memcpy(copy, wy, alloc_size);
    memcpy(wx, x, alloc_size);
    memcpy(wy, y, alloc_size);
    iinsert2(wx, wy, size, tmp);
    for (int i = 0; i < size; i++) {
        if (copy[i] != wy[i]) {
            printf("Results are different!\n");
            printf("Input x: ["); for(int i = 0; i < size; i++) printf("%d, ", x[i]); printf("]\n");
            printf("Out 1: ["); for(int i = 0; i < size; i++) printf("%d, ", copy[i]); printf("]\n");
            printf("Out 2: ["); for(int i = 0; i < size; i++) printf("%d, ", wy[i]); printf("]\n");
            break;
        }
    }

    // Run the kernel
    printf("Running... ");
    fflush(stdout);
    start_timer();
    if (kernel == 0) {
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wy, y, alloc_size);
            iinsert0(wx, wy, size);
        }
    }
    else if (kernel == 1) {
        // This kernel is very slow, and not entirely correct...
        uint8_t *y8 = malloc((size_t)size);
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            iinsert1(wx, y8, (uint8_t)size);
        }
    }
    else if (kernel == 2) {
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wy, y, alloc_size);
            iinsert2(wx, wy, size, tmp);
        }
    }
    stop_timeri(iters);

    return 0;
}
