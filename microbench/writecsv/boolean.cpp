#include <stdint.h>  // int8_t, ...
#include <stdio.h>   // printf
#include <stdlib.h>  // srand, rand
#include <time.h>    // time
#include "writecsv.h"


static void kernel_fwrite(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  char *ch = *pch;
  *ch++ = '0' + value;
  *pch = ch;
}

static void kernel_fwrite2(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  char *ch = *pch;
  *ch++ = '0' + (value == 1);
  *pch = ch - (value == NA_I1);
}


static void kernel_simple(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  **pch = value + '0';
  (*pch)++;
}


static void kernel_nonacheck(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  **pch = (char)value + '0';
  *pch += (value != NA_I1);
}


static void kernel_ge0(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  **pch = (char)value + '0';
  *pch += (value >= 0);
}


static void kernel_sprintf(char **pch, Column *col, int64_t row) {
  int8_t value = ((int8_t*) col->data)[row];
  if (value == NA_I1) return;
  *pch += sprintf(*pch, "%d", value);
}




//=================================================================================================
// Main
//=================================================================================================

BenchmarkSuite prepare_bench_boolean(int64_t N)
{
  // Prepare data array
  srand((unsigned) time(NULL));
  int8_t *data = (int8_t*) malloc(N);
  for (int64_t i = 0; i < N; i++) {
    int x = rand();
    data[i] = (x&7)==0? NA_I1 : (x&1);
  }

  // Prepare output buffer
  char *out = (char*) malloc(N * 2 + 1);
  Column *column = (Column*) malloc(sizeof(Column));
  column->data = (void*)data;

  static Kernel kernels[] = {
    { &kernel_simple,    "simple" },    //  6.615
    { &kernel_nonacheck, "nonacheck" }, //  5.573
    { &kernel_ge0,       "val >= 0" },  //  5.485
    { &kernel_fwrite,    "fwrite" },    //  7.750
    { &kernel_fwrite2,   "fwrite2" },   //  6.787
    { &kernel_sprintf,   "sprintf" },   // 63.112
    { NULL, NULL },
  };

  return {
    .column = column,
    .output = out,
    .kernels = kernels,
  };
}
