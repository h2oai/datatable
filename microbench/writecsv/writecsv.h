#ifndef WRITECSV_H
#define WRITECSV_H
#include <stdint.h>

extern double now();

#define NWRITERS 6
#define NA_I1 -128
#define NA_I2 -32768
#define NA_I4 INT32_MIN
#define NA_F8 ((double)NAN)


typedef struct Column {
  void *data;
  char *strbuf;
} Column;

typedef void (*kernel_fn)(char **output, Column *col, int64_t row);

typedef struct Kernel {
  kernel_fn kernel;
  const char *name;
} Kernel;

typedef struct BenchmarkSuite {
  Column *column;
  char *output;
  Kernel *kernels;

} BenchmarkSuite;


BenchmarkSuite prepare_bench_boolean(int64_t N);
BenchmarkSuite prepare_bench_int8(int64_t N);
BenchmarkSuite prepare_bench_int16(int64_t N);
BenchmarkSuite prepare_bench_int32(int64_t N);
BenchmarkSuite prepare_bench_double(int64_t N);
BenchmarkSuite prepare_bench_string(int64_t N);


#endif
