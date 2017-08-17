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

static int *base = NULL;


static void iinsert0(int *x, int *o, int n, int i0)
{
    int i, j, xi, oi;
    for (i = i0; i < n; i++) {
        xi = x[i];
        if (xi < x[i-1]) {
            j = i - 1;
            oi = o[i];
            while (j >= 0 && xi < x[j]) {
                x[j+1] = x[j];
                o[j+1] = o[j];
                j--;
            }
            x[j+1] = xi;
            o[j+1] = oi;
        }
    }
}

static int compute_minrun(int n)
{
    int b = 0;
    // Testing manually, I find that MR=16 has a slight lead over MR=8, and
    // significantly better than MR=4, MR=32 or MR=64.
    while (n >= 16) {
        b |= n & 1;
        n >>= 1;
    }
    return n + b;
}



// Top-down mergesort
static void mergesort0(int *x, int *o, int n, int *restrict t, int *restrict u)
{
    if (n <= 2) {
        if (n == 2 && x[0] > x[1]) {
            int t = o[0];
            o[0] = o[1];
            o[1] = t;
            t = x[0];
            x[0] = x[1];
            x[1] = t;
        }
        return;
    }
    // Sort each part recursively
    int n1 = n / 2;
    int n2 = n - n1;
    mergesort0(x, o, n1, t, u);
    mergesort0(x + n1, o + n1, n2, t + n1, u + n1);

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
}


// Bottom-up merge sort
// For now asume n is a power of 2
static void mergesort1(int *x, int *o, int n, int *restrict t, int *restrict u)
{
    // printf("mergesort1(x=%p, o=%p, n=%d)\n", x, o, n);
    int minrun = compute_minrun(n);
    // printf("  minrun = %d\n", minrun);
    // printf("  x = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");

    // First, sort all minruns in-place
    // printf("  sorting all minruns...\n");
    for (int i = 0, nleft = n; nleft > 0; i += minrun, nleft -= minrun) {
        int nn = nleft >= minrun? minrun : nleft;
        iinsert0(x + i, o + i, nn, 1);
    }
    // printf("  x = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");

    // When flip is 0, the data is in `x` / `o`; if 1 then data is in `t` / `u`
    int flip = 0;
    int *ix, *ox, *io, *oo;
    for (int wA = minrun; wA < n; wA *= 2) {
        if (flip) {
            ix = t; ox = x;
            io = u; oo = o;
            flip = 0;
        } else {
            ix = x; ox = t;
            io = o; oo = u;
            flip = 1;
        }
        int wB = wA;
        // printf("  wA = %d\n", wA);
        for (int s = 0; s < n; s += 2*wA) {
            int *restrict xA = ix + s;
            int *restrict xB = ix + s + wA;
            int *restrict oA = io + s;
            int *restrict oB = io + s + wA;
            int *restrict xR = ox + s;
            int *restrict oR = oo + s;
            if (s + 2*wA > n) {
                if (s + wA >= n) {
                    size_t sz = (n - s) * sizeof(int);
                    memcpy(xR, xA, sz);
                    memcpy(oR, oA, sz);
                    break;
                }
                wB = n - (s + wA);
            }
            // printf("    s=%d..%d, wB=%d\n", s, s+wA+wB, wB);

            int i = 0, j = 0, k = 0;
            while (1) {
                if (xA[i] <= xB[j]) {
                    xR[k] = xA[i];
                    oR[k] = oA[i];
                    k++; i++;
                    if (i == wA) {
                        size_t sz = (wB - j) * sizeof(int);
                        memcpy(xR + k, xB + j, sz);
                        memcpy(oR + k, oB + j, sz);
                        break;
                    }
                } else {
                    xR[k] = xB[j];
                    oR[k] = oB[j];
                    k++; j++;
                    if (j == wB) {
                        size_t sz = (wA - i) * sizeof(int);
                        memcpy(xR + k, xA + i, sz);
                        memcpy(oR + k, oA + i, sz);
                        break;
                    }
                }
            }
        }
    }

    if (ox != x) {
        // printf("  flipping...\n");
        memcpy(o, oo, n * sizeof(int));
        memcpy(x, ox, n * sizeof(int));
    }
    // printf("  res = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");
    // printf("  done.\n");
}



//==============================================================================
// TimSort
//==============================================================================

static int find_next_run_length(int *x, int *o, int n)
{
    if (n == 1) return 1;
    int xlast = x[1];
    int i = 2;
    if (x[0] <= xlast) {
        for (; i < n; i++) {
            int xi = x[i];
            if (xi < xlast) break;
            xlast = xi;
        }
    } else {
        for (; i < n; i++) {
            int xi = x[i];
            if (xi >= xlast) break;
            xlast = xi;
        }
        // Reverse direction of the run
        for (int j1 = 0, j2 = i - 1; j1 < j2; j1++, j2--) {
            int t = x[j1];
            x[j1] = x[j2];
            x[j2] = t;
            t = o[j1];
            o[j1] = o[j2];
            o[j2] = t;
        }
    }
    return i;
}


static void merge_chunks(int *x, int *o, int nA, int nB, int *t, int *u)
{
    memcpy(t, x, nA * sizeof(int));
    memcpy(u, o, nA * sizeof(int));
    int *restrict xA = t;
    int *restrict xB = x + nA;
    int *restrict oA = u;
    int *restrict oB = o + nA;

    int iA = 0, iB = 0, k = 0;
    while (1) {
        if (xA[iA] <= xB[iB]) {
            x[k] = xA[iA];
            o[k] = oA[iA];
            k++; iA++;
            if (iA == nA) {
                break;
            }
        } else {
            x[k] = xB[iB];
            o[k] = oB[iB];
            k++; iB++;
            if (iB == nB) {
                memcpy(x + k, xA + iA, (nA - iA) * sizeof(int));
                memcpy(o + k, oA + iA, (nA - iA) * sizeof(int));
                break;
            }
        }
    }
}


static void merge_stack(int *stack, int *stacklen, int *x, int *o, int *tmp1, int *tmp2)
{
    int sn = *stacklen;
    while (sn >= 3) {
        int iA = stack[sn-3];
        int iB = stack[sn-2];
        int iC = stack[sn-1];
        int iL = stack[sn];
        int nA = iB - iA;
        int nB = iC - iB;
        int nC = iL - iC;
        if (nA && nA <= nB + nC) {
            // Invariant |A| > |B| + |C| is violated
            if (nA < nC) {  // merge A and B
                merge_chunks(x + iA, o + iA, nA, nB, tmp1, tmp2);
                stack[sn-2] = iC;
                stack[sn-1] = iL;
            } else {  // merge B and C
                merge_chunks(x + iB, o + iB, nB, nC, tmp1, tmp2);
                stack[sn-1] = iL;
            }
        } else if (nB <= nC) {
            // Invariant |B| > |C| is violated: merge B and C
            merge_chunks(x + iB, o + iB, nB, nC, tmp1, tmp2);
            stack[sn-1] = iL;
        } else
            break;
        sn--;
    }
    *stacklen = sn;
}

static void final_merge_stack(int *stack, int *stacklen, int *x, int *o, int *tmp1, int *tmp2)
{
    int sn = *stacklen;
    while (sn >= 3) {
        int iB = stack[sn-2];
        int iC = stack[sn-1];
        int iL = stack[sn];
        merge_chunks(x + iB, o + iB, iC - iB, iL - iC, tmp1, tmp2);
        stack[sn-1] = iL;
        sn--;
    }
    *stacklen = sn;
}


static void timsort(int *x, int *o, int n, int *tmp1, int *tmp2)
{
    // printf("timsort(x=%p, o=%p, n=%d, tmp1=%p, tmp2=%p)\n", x, o, n, tmp1, tmp2);
    // printf("  x = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");
    int minrun = compute_minrun(n);
    // printf("  minrun = %d\n", minrun);
    int stack[85];
    stack[0] = 0;
    stack[1] = 0;
    int stacklen = 1;

    int i = 0;
    int nleft = n;
    while (nleft) {
        // printf("  [i=%d]\n", i);
        // Find the next ascending run; if it is too short then extend to
        // `min(minrun, nleft)` elements.
        int rl = find_next_run_length(x + i, o + i, nleft);
        // printf("    runL = %d\n", rl);
        if (rl < minrun) {
            int newrun = minrun <= nleft? minrun : nleft;
            iinsert0(x + i, o + i, newrun, rl);
            rl = newrun;
            // printf("    x = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");
            // printf("    runL = %d\n", rl);
        }
        // Push the run onto the stack, and then merge elements on the stack
        // if necessary
        stack[++stacklen] = i + rl;
        // printf("    stack = ["); for(int i = 0; i <= stacklen; i++) printf("%d, ", stack[i]); printf("\b\b]\n");
        // printf("    merging stack...\n");
        merge_stack(stack, &stacklen, x, o, tmp1, tmp2);
        // printf("    x = ["); for(int i = 0; i < n; i++) printf("%d, ", x[i]); printf("\b\b]\n");
        // printf("    stack = ["); for(int i = 0; i <= stacklen; i++) printf("%d, ", stack[i]); printf("\b\b]\n");
        i += rl;
        nleft -= rl;
    }
    // printf("  prepare to do final merge of the stack...\n");
    // printf("    stack = ["); for(int i = 0; i <= stacklen; i++) printf("%d, ", stack[i]); printf("\b\b]\n");
    final_merge_stack(stack, &stacklen, x, o, tmp1, tmp2);
    assert(stacklen == 2);
    // printf("  end\n");
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
    double total_time1 = 0, min_time1 = 0, max_time1 = 0;
    double total_time2 = 0, min_time2 = 0, max_time2 = 0;
    double total_time3 = 0, min_time3 = 0, max_time3 = 0;

    for (int b = 0; b < nbatches; b++)
    {
        // Prepare data array
        srand(time(NULL));
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
        mergesort1(wx, wo, size, tmp, tmp + size);
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
            mergesort0(wx, wo, size, tmp, tmp + size);
        }
        double t0 = get_timer_iter(iters);
        total_time0 += t0;
        if (b == 0 || t0 < min_time0) min_time0 = t0;
        if (b == 0 || t0 > max_time0) max_time0 = t0;

        // Kernel 1
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wo, o, alloc_size);
            mergesort1(wx, wo, size, tmp, tmp + size);
        }
        double t1 = get_timer_iter(iters);
        total_time1 += t1;
        if (b == 0 || t1 < min_time1) min_time1 = t1;
        if (b == 0 || t1 > max_time1) max_time1 = t1;

        // Kernel 2
        start_timer();
        for (int i = 0; i < iters; i++) {
            memcpy(wx, x, alloc_size);
            memcpy(wo, o, alloc_size);
            timsort(wx, wo, size, tmp, tmp + size);
        }
        double t2 = get_timer_iter(iters);
        total_time2 += t2;
        if (b == 0 || t2 < min_time2) min_time2 = t2;
        if (b == 0 || t2 > max_time2) max_time2 = t2;
    }

    printf("@ mergesort0:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time0 / nbatches, min_time0, max_time0);

    printf("@ mergesort1:  mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time1 / nbatches, min_time1, max_time1);

    printf("@ timsort:     mean = %.2f ns,  min = %.2f ns, max = %.2f ns\n",
           total_time2 / nbatches, min_time2, max_time2);

    return 0;
}
