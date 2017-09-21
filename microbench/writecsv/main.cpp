#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "writecsv.h"
#include "utils.h"


// Run the benchmark
void run_bench(int B, int64_t N, BenchmarkSuite bench)
{
  int nkernels = 0, maxnamelen = 0;
  while (bench.kernels[nkernels].kernel) {
    int len = strlen(bench.kernels[nkernels].name);
    if (len > maxnamelen) maxnamelen = len;
    nkernels++;
  }

  Column *column = bench.column;
  for (int k = 0; k < nkernels; k++) {
    kernel_fn kernel = bench.kernels[k].kernel;

    double t0 = now();
    for (int b = 0; b < B; b++) {
      char *pch = bench.output;
      for (int64_t i = 0; i < N; i++) {
        kernel(&pch, column, i);
        *pch++ = ',';
      }
    }
    double t1 = now();
    bench.output[120] = 0;
    printf("[%d] %-*s: %7.3f ns  [out: %s]\n",
           k, maxnamelen, bench.kernels[k].name, (t1-t0)*1e9/B/N, bench.output);
  }

  // Clean up
  // free(bench.output);
  // free(column->data);
  // free(column);
}


int main(int argc, char **argv)
{
  int A = getCmdArgInt(argc, argv, "writer", 1);
  int B = getCmdArgInt(argc, argv, "batches", 100);
  int N = getCmdArgInt(argc, argv, "n", 64);
  if (A <= 0 || A > NWRITERS) {
    printf("Invalid writer: %d  (max writers=%d)\n", A, NWRITERS);
    return 1;
  }

  static const char *writer_names[NWRITERS+1] = {
    "",
    "boolean",
    "int8",
    "int16",
    "int32",
    "double",
    "string",
  };

  printf("Writer  = %s\n", writer_names[A]);
  printf("Batches = %d\n", B);
  printf("Numrows = %d\n", N);
  printf("\n");

  switch (A) {
    case 1: run_bench(B, N, prepare_bench_boolean(N)); break;
    case 2: run_bench(B, N, prepare_bench_int8(N)); break;
    case 3: run_bench(B, N, prepare_bench_int16(N)); break;
    case 4: run_bench(B, N, prepare_bench_int32(N)); break;
    case 5: run_bench(B, N, prepare_bench_double(N)); break;
    case 6: run_bench(B, N, prepare_bench_string(N)); break;
  }
}
