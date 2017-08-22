#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sort.h"
#include "utils.h"

int *tmp1 = NULL;
int *tmp2 = NULL;


int test(const char *algoname, sortfn_t sortfn, int N, int K, int B, int T)
{
    size_t alloc_size = (size_t)N * sizeof(int);
    int *x = malloc(alloc_size);  // data to be sorted
    int *o = malloc(alloc_size);  // ordering, sorted together with the data
    int *wx = NULL, *wo = NULL;
    if (!x || !o) return 1;

    int niters = 0;
    double *ts = malloc(B * sizeof(double));
    for (int b = 0; b < B; b++)
    {
        //----- Prepare data array -------------------------
        srand(time(NULL));
        int mask = (1 << K);
        for (int i = 0; i < N; i++) {
            x[i] = rand() % mask;
            o[i] = i;
        }

        //----- Determine the number of iterations ---------
        if (niters == 0) {
            niters = 1;
            while (1) {
                wx = realloc(wx, alloc_size * niters);
                wo = realloc(wo, alloc_size * niters);
                if (!wx || !wo) return 1;
                for (int i = 0; i < niters; i++) {
                    memcpy(wx + i * N, x, alloc_size);
                    memcpy(wo + i * N, o, alloc_size);
                }
                double t0 = now();
                for (int i = 0; i < niters; i++) {
                    int *xx = wx + i * N;
                    int *oo = wo + i * N;
                    sortfn(xx, oo, N, K);
                }
                double t1 = now();
                if (t1 - t0 > 1e-3) {
                    double time_per_iter = (t1 - t0) / niters;
                    niters = (int)(0.99 + T * 1e-3 / (time_per_iter * B));
                    if (niters < 1) niters = 1;
                    wx = realloc(wx, alloc_size * niters);
                    wo = realloc(wo, alloc_size * niters);
                    if (!wx || !wo) return 1;
                    break;
                }
                niters *= 2;
            }
        }

        //----- Run the iterations -------------------------
        for (int i = 0; i < niters; i++) {
            memcpy(wx + i * N, x, alloc_size);
            memcpy(wo + i * N, o, alloc_size);
        }
        double t0 = now();
        for (int i = 0; i < niters; i++) {
            int *xx = wx + i * N;
            int *oo = wo + i * N;
            sortfn(xx, oo, N, K);
        }
        double t1 = now();
        ts[b] = (t1 - t0) / niters;
    }

    //----- Process time stats -----------------------------
    double min1, min2, max1, max2;
    if (ts[0] < ts[1]) {
        min1 = max2 = ts[0];
        min2 = max1 = ts[1];
    } else {
        min1 = max2 = ts[1];
        min2 = max1 = ts[0];
    }
    double sumt = 0;
    for (int b = 0; b < B; b++) {
        double t = ts[b];
        sumt += t;
        if (t < min1) {
            min2 = min1;
            min1 = t;
        } else if (t < min2) {
            min2 = t;
        }
        if (t > max1) {
            max2 = max1;
            max1 = t;
        } else if (t > max2) {
            max2 = t;
        }
    }
    double tavg = (sumt - min1 - min2 - max1 - max2) / (B - 4);
    printf("@%s:  %.2f ns\n", algoname, tavg * 1e9);
    free(x);
    free(o);
    free(wx);
    free(wo);
    return 0;
}


int main(int argc, char **argv)
{
    // A - which set of algorithms to run: 1 (insert sorts), 2 (merge sorts),
    //     3 (radix sort), 4 (combo)
    // B - number of batches, i.e. how many different datasets to try. Default
    //     is 100, minimum num batches is 5.
    // K - number of significant bits, i.e. each dataset will be comprised of
    //     random integers in the range [0, 1<<K)
    // N - array size
    // T - how long (in ms) to run the test for each algo, approximately.
    int A = getCmdArgInt(argc, argv, "algo", 1);
    int B = getCmdArgInt(argc, argv, "batches", 100);
    int N = getCmdArgInt(argc, argv, "n", 64);
    int K = getCmdArgInt(argc, argv, "k", 16);
    int T = getCmdArgInt(argc, argv, "time", 1000);
    if (B < 5) B = 5;
    printf("Array size = %d\n", N);
    printf("N sig bits = %d\n", K);
    printf("N batches  = %d\n", B);
    printf("Exec. time = %d ms\n", T);
    printf("\n");

    switch (A) {
        case 1:
            tmp1 = malloc(2 * (size_t)N * sizeof(int));
            test("insert0", iinsert0, N, K, B, T);
            test("insert2", iinsert2, N, K, B, T);
            test("insert3", iinsert3, N, K, B, T);
            free(tmp1);
            break;

        case 2:
            tmp1 = malloc((size_t)N * sizeof(int));
            tmp2 = malloc((size_t)N * sizeof(int));
            test("mergeTD", mergesort0, N, K, B, T);
            test("mergeBU", mergesort1, N, K, B, T);
            test("timsort", timsort, N, K, B, T);
            free(tmp1);
            free(tmp2);
            break;

        case 3:
            tmp1 = malloc((size_t)(1 << K) * sizeof(int));
            tmp2 = malloc((size_t)N * sizeof(int));
            test("radixsort", radixsort0, N, K, B, T);
            free(tmp1);
            free(tmp2);
            break;

        default:
            printf("A = %d is not supported\n", A);
    }

    return 0;
}
