#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "sort.h"
#include "utils.h"

int tmp0 = 0;
int *tmp1 = NULL;
int *tmp2 = NULL;
int *tmp3 = NULL;



#define addptr(p, s)  ((void*)((char*)(p) + (size_t)(s)))


int test(const char *algoname, sortfn_t sortfn, int S, int N, int K, int B, int T)
{
    assert(K <= S*8);
    size_t alloc_size_x = (size_t)N * S;
    size_t alloc_size_o = (size_t)N * sizeof(int);
    void *x = malloc(alloc_size_x);  // data to be sorted
    int  *o = malloc(alloc_size_o);  // ordering, sorted together with the data
    void *wx = NULL;
    int  *wo = NULL;
    if (!x || !o) return 1;

    int niters = 0;
    double *ts = malloc(B * sizeof(double));
    double tsum = 0;
    for (int b = 0; b < B; b++)
    {
        //----- Prepare data array -------------------------
        srand(time(NULL));
        if (S == 1) {
            uint8_t mask = (uint8_t)((1 << K) - 1);
            uint8_t *xx = (uint8_t*) x;
            for (int i = 0; i < N; i++) {
                xx[i] = (uint8_t)(rand() & mask);
                o[i] = i;
            }
        } else if (S == 2) {
            uint16_t mask = (uint16_t)((1 << K) - 1);
            uint16_t *xx = (uint16_t*) x;
            for (int i = 0; i < N; i++) {
                xx[i] = (uint16_t)(rand() & mask);
                o[i] = i;
            }
        } else if (S == 4) {
            // If K=32, mask should evaluate to 0xFFFFFFFF
            uint32_t mask = (uint32_t)((1 << K) - 1);
            uint32_t *xx = (uint32_t*) x;
            for (int i = 0; i < N; i++) {
                xx[i] = (uint32_t)(rand() & mask);
                o[i] = i;
            }
        } else assert(0);

        //----- Determine the number of iterations ---------
        if (niters == 0) {
            niters = 1;
            while (1) {
                wx = realloc(wx, alloc_size_x * niters);
                wo = realloc(wo, alloc_size_o * niters);
                if (!wx || !wo) return 1;
                if (N >= 32768) break;  // if N is large enough, use niters = 1
                for (int i = 0; i < niters; i++) {
                    memcpy(addptr(wx, i * N * S), x, alloc_size_x);
                    memcpy(addptr(wo, i * N * 4), o, alloc_size_o);
                }
                double t0 = now();
                for (int i = 0; i < niters; i++) {
                    void *xx = addptr(wx, i * N * S);
                    int  *oo = addptr(wo, i * N * 4);
                    sortfn(xx, oo, N, K);
                }
                double t1 = now();
                if (t1 - t0 > 1e-3) {
                    double time_per_iter = (t1 - t0) / niters;
                    niters = (int)(0.99 + T * 1e-3 / (time_per_iter * B));
                    if (niters < 1) niters = 1;
                    wx = realloc(wx, alloc_size_x * niters);
                    wo = realloc(wo, alloc_size_o * niters);
                    if (!wx || !wo) return 1;
                    break;
                }
                niters *= 2;
            }
        }

        //----- Run the iterations -------------------------
        for (int i = 0; i < niters; i++) {
            memcpy(addptr(wx, i * N * S), x, alloc_size_x);
            memcpy(addptr(wo, i * N * 4), o, alloc_size_o);
        }
        double t0 = now();
        for (int i = 0; i < niters; i++) {
            void *xx = addptr(wx, i * N * S);
            int  *oo = addptr(wo, i * N * 4);
            sortfn(xx, oo, N, K);
        }
        double t1 = now();
        ts[b] = (t1 - t0) / niters;
        tsum += ts[b];
        if ((tsum * 1000 > T && b >= 2) || tsum * 1000 > T * 3) {
            B = b + 1;
            break;
        }
    }

    //----- Process time stats -----------------------------
    double tavg;
    if (B >= 10) {
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
        tavg = (sumt - min1 - min2 - max1 - max2) / (B - 4);
    } else {
        double sumt = 0;
        for (int b = 0; b < B; b++) sumt += ts[b];
        tavg = sumt / B;
    }
    printf("@%s:  %.3f ns\n", algoname, tavg * 1e9);
    // printf("Freeing x=%p, o=%p, wx=%p, wo=%p\n", x, o, wx, wo);
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
    //     is 100.
    // K - number of significant bits, i.e. each dataset will be comprised of
    //     random integers in the range [0, 1<<K)
    // N - array size
    // T - how long (in ms) to run the test for each algo, approximately.
    int A = getCmdArgInt(argc, argv, "algo", 1);
    int B = getCmdArgInt(argc, argv, "batches", 100);
    int N = getCmdArgInt(argc, argv, "n", 64);
    int K = getCmdArgInt(argc, argv, "k", 16);
    int T = getCmdArgInt(argc, argv, "time", 1000);
    if (N <= 16 && A == 4) B *= 10;
    printf("Array size = %d\n", N);
    printf("N sig bits = %d\n", K);
    printf("N batches  = %d\n", B);
    printf("Exec. time = %d ms\n", T);
    printf("\n");

    char name[100];
    tmp1 = malloc((size_t)(2 * N) * sizeof(int));
    tmp2 = malloc((size_t)(N) * sizeof(int));
    tmp3 = malloc((size_t)(1 << K) * sizeof(int));
    if (!tmp1 || !tmp2 || !tmp3) return 2;
    // printf("Allocated tmps: tmp1=%p, tmp2=%p, tmp3=%p\n", tmp1, tmp2, tmp3);

    switch (A) {
        case 1:
            test("4:insert0", (sortfn_t)iinsert0_i4, 4, N, K, B, T);
            test("4:insert2", (sortfn_t)iinsert2, 4, N, K, B, T);
            test("4:insert3", (sortfn_t)iinsert3, 4, N, K, B, T);
            if (K <= 8) {
                test("1:insert0", (sortfn_t)iinsert0_i1, 1, N, K, B, T);
                test("1:insert3", (sortfn_t)iinsert3_i1, 1, N, K, B, T);
            }
            break;

        case 2:
            test("mergeTD", (sortfn_t)mergesort0, 4, N, K, B, T);
            test("mergeBU", (sortfn_t)mergesort1, 4, N, K, B, T);
            test("timsort", (sortfn_t)timsort, 4, N, K, B, T);
            break;

        case 3: {
            if (K <= 20) {
                sprintf(name, "radix0-r%d", K);
                test(name, (sortfn_t)radixsort0, 4, N, K, B, T);
            }
            int kstep = K <= 4? 1 : K <= 8? 2 : 4;
            kstep = 8;
            for (tmp0 = kstep; tmp0 < K; tmp0 += kstep) {
                if (tmp0 > 20) break;
                sprintf(name, "radix1-%d/m", tmp0);
                test(name, (sortfn_t)radixsort1, 4, N, K, B, T);
                sprintf(name, "radix2-%d/%d", tmp0, K - tmp0);
                test(name, (sortfn_t)radixsort2, 4, N, K, B, T);
                if (K - tmp0 <= 16) {
                    sprintf(name, "radix3-%d/%d", tmp0, K - tmp0);
                    test(name, (sortfn_t)radixsort3, 4, N, K, B, T);
                }
            }
        }
        break;

        case 4: {
            if (N <= 10000) {
                test("4:insert0", (sortfn_t)iinsert0_i4, 4, N, K, B, T);
            } else {
                printf("@4:insert0: -\n");
            }
            if (N <= 1e6) test("mergeBU", (sortfn_t)mergesort1, 4, N, K, B, T);
            else printf("@mergeBU: -\n");
            int kstep = K <= 4? 1 : K <= 8? 2 : 4;
            for (tmp0 = kstep; tmp0 < K; tmp0 += kstep) {
                if (K - tmp0 > 20) continue;
                if (tmp0 > 20) continue;
                sprintf(name, "radix1-%d/m", tmp0);
                test(name, (sortfn_t)radixsort1, 4, N, K, B, T);
                sprintf(name, "radix2-%d/%d", tmp0, K - tmp0);
                test(name, (sortfn_t)radixsort2, 4, N, K, B, T);
                if (K - tmp0 <= 16) {
                    sprintf(name, "radix3-%d/%d", tmp0, K - tmp0);
                    test(name, (sortfn_t)radixsort3, 4, N, K, B, T);
                }
            }
            if (K <= 20) {
                sprintf(name, "radix%d", K);
                test(name, (sortfn_t)radixsort0, 4, N, K, B, T);
            }
        }
        break;

        case 5: {
            if (K > 8) {
                printf("This case is only available for K <= 8");
                return 5;
            }
            if (N <= 10000) {
                test("1:insert0", (sortfn_t)iinsert0_i1, 1, N, K, B, T);
                test("1:insert3", (sortfn_t)iinsert3_i1, 1, N, K, B, T);
            } else {
                printf("@1:insert0: -\n");
                printf("@1:insert3: -\n");
            }
            sprintf(name, "radix0-%d", K);
            test(name, (sortfn_t)radixsort0_i1, 1, N, K, B, T);
            int kstep = K <= 4? 1 : K <= 8? 2 : 4;
            for (tmp0 = kstep; tmp0 < K; tmp0 += kstep) {
                if (K - tmp0 > 20) continue;
                if (tmp0 > 20) continue;
                sprintf(name, "radix2-%d/o", tmp0);
                test(name, (sortfn_t)radixsort2_i1, 1, N, K, B, T);
            }
        }

        default:
            printf("A = %d is not supported\n", A);
    }

    // printf("Freeing tmp1=%p, tmp2=%p, tmp3=%p\n", tmp1, tmp2, tmp3);
    free(tmp1);
    free(tmp2);
    free(tmp3);
    return 0;
}
