#ifndef MICROBENCH_SORT_H
#define MICROBENCH_SORT_H

extern int tmp0;
extern int *tmp1;
extern int *tmp2;
extern int *tmp3;

typedef void (*sortfn_t)(int *x, int *o, int N, int K);

void iinsert0(int *x, int *o, int N, int K);
void iinsert2(int *x, int *o, int N, int K);
void iinsert3(int *x, int *o, int N, int K);
void mergesort0(int *x, int *o, int N, int K);
void mergesort1(int *x, int *o, int n, int K);
void timsort(int *x, int *o, int n, int K);
void radixsort0(int *x, int *o, int n, int K);
void radixsort1(int *x, int *o, int n, int K);
void radixsort2(int *x, int *o, int n, int K);

#endif
